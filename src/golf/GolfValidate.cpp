#include "GolfValidate.h"

#if defined(CROSSPOINT_GOLF)

GolfValidationResult validateGolfRound(GolfRound& round) {
  GolfValidationResult result{round.holeCount == 9 || round.holeCount == 18, false, 0, 0};
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
    if (round.strokes[hole] == 0) {
      round.putts[hole] = 0;
      round.in100[hole] = 0;
    } else {
      const uint16_t shortShots = static_cast<uint16_t>(round.putts[hole]) + round.in100[hole];
      if (shortShots > round.strokes[hole]) {
        uint16_t excess = shortShots - round.strokes[hole];
        const uint8_t in100Reduction = excess < round.in100[hole] ? static_cast<uint8_t>(excess) : round.in100[hole];
        round.in100[hole] -= in100Reduction;
        excess -= in100Reduction;
        round.putts[hole] -= static_cast<uint8_t>(excess);
      }
    }
    if (round.putts[hole] != oldPutts) {
      result.puttsRepaired |= 1UL << hole;
    }
    if (round.in100[hole] != oldIn100) {
      result.in100Repaired |= 1UL << hole;
    }
  }
  return result;
}

#endif
