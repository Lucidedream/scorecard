#include "GolfRules.h"

#if defined(CROSSPOINT_GOLF)

namespace {

constexpr uint8_t MAX_STROKES = 99;

constexpr uint8_t fieldBit(GolfField field) { return 1u << static_cast<uint8_t>(field); }

GolfMutationResult clamped() { return {false, false, 0, false}; }

GolfMutationResult changed(bool autoBumpedStrokes = false) { return {true, false, 0, autoBumpedStrokes}; }

bool isValidHole(const GolfRound& round, uint8_t hole) { return hole < round.holeCount && hole < GolfRound::MAX_HOLES; }

GolfMutationResult blockedByFloor(const GolfRound& round, uint8_t hole) {
  uint8_t blockingFields = 0;
  if (round.putts[hole] != 0) {
    blockingFields |= fieldBit(GolfField::Putts);
  }
  if (round.in100[hole] != 0) {
    blockingFields |= fieldBit(GolfField::In100);
  }
  return {false, true, blockingFields, false};
}

}  // namespace

GolfMutationResult incrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field) {
  if (!isValidHole(round, hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Strokes:
      if (round.strokes[hole] >= MAX_STROKES) {
        return clamped();
      }
      ++round.strokes[hole];
      return changed();

    case GolfField::Putts:
    case GolfField::In100: {
      uint8_t& counter = field == GolfField::Putts ? round.putts[hole] : round.in100[hole];
      const uint16_t shortShots = static_cast<uint16_t>(round.putts[hole]) + round.in100[hole];
      if (shortShots >= round.strokes[hole]) {
        if (round.strokes[hole] >= MAX_STROKES) {
          return clamped();
        }
        ++round.strokes[hole];
        ++counter;
        return changed(true);
      }
      ++counter;
      return changed();
    }
  }

  return clamped();
}

GolfMutationResult decrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field) {
  if (!isValidHole(round, hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Strokes: {
      if (round.strokes[hole] == 0) {
        return clamped();
      }
      const uint16_t shortShots = static_cast<uint16_t>(round.putts[hole]) + round.in100[hole];
      if (static_cast<uint16_t>(round.strokes[hole] - 1) < shortShots) {
        return blockedByFloor(round, hole);
      }
      --round.strokes[hole];
      return changed();
    }

    case GolfField::Putts:
      if (round.putts[hole] == 0) {
        return clamped();
      }
      --round.putts[hole];
      return changed();

    case GolfField::In100:
      if (round.in100[hole] == 0) {
        return clamped();
      }
      --round.in100[hole];
      return changed();
  }

  return clamped();
}

GolfField nextGolfField(GolfField field) {
  switch (field) {
    case GolfField::Strokes:
      return GolfField::Putts;
    case GolfField::Putts:
      return GolfField::In100;
    case GolfField::In100:
      return GolfField::Strokes;
  }
  return GolfField::Strokes;
}

#endif
