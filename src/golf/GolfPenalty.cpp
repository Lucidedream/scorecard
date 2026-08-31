#include "GolfPenalty.h"

#if defined(CROSSPOINT_GOLF)

namespace {

constexpr uint8_t FIELD_MASK = 0x03;
constexpr uint8_t KIND_MASK = 0x04;
constexpr uint8_t RESERVED_MASK = 0x08;

bool validHole(const GolfRound& round, const uint8_t hole) {
  return hole < round.holeCount && hole < GolfRound::MAX_HOLES;
}

uint8_t packedAt(const GolfRound& round, const uint8_t hole, const uint8_t index) {
  const uint8_t packed = round.penaltyEvents[hole][index / 2];
  return index % 2 == 0 ? static_cast<uint8_t>(packed & 0x0f) : static_cast<uint8_t>(packed >> 4);
}

void storeAt(GolfRound& round, const uint8_t hole, const uint8_t index, const uint8_t packed) {
  uint8_t& target = round.penaltyEvents[hole][index / 2];
  const uint8_t shift = index % 2 == 0 ? 0 : 4;
  target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
}

}  // namespace

uint8_t golfPackPenaltyEvent(const GolfField field, const GolfPenaltyKind kind) {
  return static_cast<uint8_t>(static_cast<uint8_t>(field) | (static_cast<uint8_t>(kind) << 2));
}

bool golfUnpackPenaltyEvent(const uint8_t packed, GolfPenaltyEvent& event) {
  if ((packed & RESERVED_MASK) != 0 || (packed & FIELD_MASK) > static_cast<uint8_t>(GolfField::Out100)) return false;
  event.field = static_cast<GolfField>(packed & FIELD_MASK);
  event.kind = static_cast<GolfPenaltyKind>((packed & KIND_MASK) >> 2);
  return true;
}

bool golfPenaltyEventAt(const GolfRound& round, const uint8_t hole, const uint8_t index, GolfPenaltyEvent& event) {
  return validHole(round, hole) && index < round.penaltyCount[hole] && index < GolfRound::MAX_PENALTIES_PER_HOLE &&
         golfUnpackPenaltyEvent(packedAt(round, hole, index), event);
}

GolfPenaltyMutationStatus golfAppendPenalty(GolfRound& round, const uint8_t hole, const GolfField field,
                                            const GolfPenaltyKind kind) {
  if (!validHole(round, hole)) return GolfPenaltyMutationStatus::InvalidHole;
  if (static_cast<uint8_t>(field) > static_cast<uint8_t>(GolfField::Out100) ||
      static_cast<uint8_t>(kind) > static_cast<uint8_t>(GolfPenaltyKind::Ob)) {
    return GolfPenaltyMutationStatus::InvalidEvent;
  }
  if (round.penaltyCount[hole] >= GolfRound::MAX_PENALTIES_PER_HOLE) return GolfPenaltyMutationStatus::HoleFull;

  const GolfMutationResult mutation = incrementGolfCounter(round, hole, field);
  if (!mutation.changed) return GolfPenaltyMutationStatus::CounterClamped;
  if (field == GolfField::Putts && !mutation.carriedIn100) {
    if (!incrementGolfCounter(round, hole, GolfField::In100).changed) {
      decrementGolfCounter(round, hole, GolfField::Putts);
      return GolfPenaltyMutationStatus::CounterClamped;
    }
  }
  storeAt(round, hole, round.penaltyCount[hole], golfPackPenaltyEvent(field, kind));
  ++round.penaltyCount[hole];
  return GolfPenaltyMutationStatus::Changed;
}

GolfPenaltyMutationStatus golfRemoveLatestPenalty(GolfRound& round, const uint8_t hole, const GolfField field) {
  if (!validHole(round, hole)) return GolfPenaltyMutationStatus::InvalidHole;
  if (static_cast<uint8_t>(field) > static_cast<uint8_t>(GolfField::Out100)) {
    return GolfPenaltyMutationStatus::InvalidEvent;
  }

  uint8_t removeIndex = round.penaltyCount[hole];
  while (removeIndex > 0) {
    GolfPenaltyEvent event{};
    --removeIndex;
    if (golfUnpackPenaltyEvent(packedAt(round, hole, removeIndex), event) && event.field == field) break;
  }
  GolfPenaltyEvent removed{};
  if (removeIndex >= round.penaltyCount[hole] ||
      !golfUnpackPenaltyEvent(packedAt(round, hole, removeIndex), removed) || removed.field != field) {
    return GolfPenaltyMutationStatus::NoMarker;
  }

  GolfMutationResult mutation{};
  if (field == GolfField::Putts && round.putts[hole] == round.in100[hole]) {
    mutation = decrementGolfCounter(round, hole, GolfField::In100);
  } else if (field == GolfField::Putts) {
    mutation = decrementGolfCounter(round, hole, GolfField::Putts);
    if (mutation.changed && !decrementGolfCounter(round, hole, GolfField::In100).changed) {
      incrementGolfCounter(round, hole, GolfField::Putts);
      return GolfPenaltyMutationStatus::CounterClamped;
    }
  } else {
    mutation = decrementGolfCounter(round, hole, field);
  }
  if (!mutation.changed) return GolfPenaltyMutationStatus::CounterClamped;

  for (uint8_t index = removeIndex; index + 1 < round.penaltyCount[hole]; ++index) {
    storeAt(round, hole, index, packedAt(round, hole, static_cast<uint8_t>(index + 1)));
  }
  --round.penaltyCount[hole];
  storeAt(round, hole, round.penaltyCount[hole], 0);
  return GolfPenaltyMutationStatus::Changed;
}

uint8_t golfHazardsForHole(const GolfRound& round, const uint8_t hole) {
  if (!validHole(round, hole)) return 0;
  uint8_t hazards = 0;
  const uint8_t count = round.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? round.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (golfUnpackPenaltyEvent(packedAt(round, hole, index), event) && event.kind == GolfPenaltyKind::Hazard) ++hazards;
  }
  return hazards;
}

uint8_t golfObsForHole(const GolfRound& round, const uint8_t hole) {
  if (!validHole(round, hole)) return 0;
  uint8_t obs = 0;
  const uint8_t count = round.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? round.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (golfUnpackPenaltyEvent(packedAt(round, hole, index), event) && event.kind == GolfPenaltyKind::Ob) ++obs;
  }
  return obs;
}

uint16_t golfHazardsForRound(const GolfRound& round) {
  uint16_t total = 0;
  const uint8_t holes = round.holeCount < GolfRound::MAX_HOLES ? round.holeCount : GolfRound::MAX_HOLES;
  for (uint8_t hole = 0; hole < holes; ++hole) total += golfHazardsForHole(round, hole);
  return total;
}

uint16_t golfObsForRound(const GolfRound& round) {
  uint16_t total = 0;
  const uint8_t holes = round.holeCount < GolfRound::MAX_HOLES ? round.holeCount : GolfRound::MAX_HOLES;
  for (uint8_t hole = 0; hole < holes; ++hole) total += golfObsForHole(round, hole);
  return total;
}

uint16_t golfPenaltyStrokesForHole(const GolfRound& round, const uint8_t hole) {
  return static_cast<uint16_t>(golfHazardsForHole(round, hole)) + static_cast<uint16_t>(golfObsForHole(round, hole)) * 2;
}

uint16_t golfPenaltyStrokesForRound(const GolfRound& round) {
  return static_cast<uint16_t>(golfHazardsForRound(round) + golfObsForRound(round) * 2);
}

#endif
