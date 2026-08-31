#include "GolfValidate.h"

#if defined(CROSSPOINT_GOLF)

#include "GolfPenalty.h"

namespace {

uint8_t packedAt(const GolfRound& round, const uint8_t hole, const uint8_t index) {
  const uint8_t packed = round.penaltyEvents[hole][index / 2];
  return index % 2 == 0 ? static_cast<uint8_t>(packed & 0x0f) : static_cast<uint8_t>(packed >> 4);
}

void storeAt(GolfRound& round, const uint8_t hole, const uint8_t index, const uint8_t packed) {
  uint8_t& target = round.penaltyEvents[hole][index / 2];
  const uint8_t shift = index % 2 == 0 ? 0 : 4;
  target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
}

uint8_t fieldShotCount(const GolfRound& round, const uint8_t hole, const GolfField field) {
  switch (field) {
    case GolfField::Putts:
      return round.putts[hole];
    case GolfField::In100:
      return round.in100[hole];
    case GolfField::Out100:
      return round.out100[hole];
  }
  return 0;
}

}  // namespace

GolfValidationResult validateGolfRound(GolfRound& round) {
  GolfValidationResult result{round.holeCount == 9 || round.holeCount == 18, false, 0, 0, 0, 0, 0};
  if (!result.valid) {
    return result;
  }

  if (round.currentHole >= round.holeCount) {
    round.currentHole = 0;
    result.currentHoleReset = true;
  }

  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    const uint8_t oldPutts = round.putts[hole];
    const uint8_t oldIn100 = round.in100[hole];
    if (round.in100[hole] == 0 && round.out100[hole] == 0) {
      round.putts[hole] = 0;
    } else if (round.putts[hole] > round.in100[hole]) {
      round.putts[hole] = round.in100[hole];
    }
    if (round.putts[hole] != oldPutts) {
      result.puttsRepaired |= 1UL << hole;
    }
    if (round.in100[hole] != oldIn100) result.in100Repaired |= 1UL << hole;

    const uint8_t originalCount = round.penaltyCount[hole];
    const uint8_t readableCount =
        originalCount < GolfRound::MAX_PENALTIES_PER_HOLE ? originalCount : GolfRound::MAX_PENALTIES_PER_HOLE;
    if (originalCount > GolfRound::MAX_PENALTIES_PER_HOLE) result.penaltyCountRepaired |= 1UL << hole;

    uint8_t fieldMarkers[3]{};
    uint8_t repairedCount = 0;
    for (uint8_t index = 0; index < readableCount; ++index) {
      GolfPenaltyEvent event{};
      const uint8_t packed = packedAt(round, hole, index);
      if (!golfUnpackPenaltyEvent(packed, event)) {
        result.penaltyEventRepaired |= 1UL << hole;
        continue;
      }
      const uint8_t field = static_cast<uint8_t>(event.field);
      if (fieldMarkers[field] >= fieldShotCount(round, hole, event.field)) {
        result.penaltyMarkerRepaired |= 1UL << hole;
        continue;
      }
      ++fieldMarkers[field];
      storeAt(round, hole, repairedCount++, packed);
    }
    if (repairedCount != readableCount) result.penaltyCountRepaired |= 1UL << hole;
    round.penaltyCount[hole] = repairedCount;
    while (repairedCount < GolfRound::MAX_PENALTIES_PER_HOLE) storeAt(round, hole, repairedCount++, 0);
  }
  return result;
}

#endif
