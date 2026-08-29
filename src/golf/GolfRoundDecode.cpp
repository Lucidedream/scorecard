#include "GolfRoundDecode.h"

#if defined(CROSSPOINT_GOLF)

GolfRoundDecodeStatus golfCheckRound(GolfRound& out, const int version, const int holes, const int currentHole,
                                     const GolfRoundColumnLengths& lengths, GolfValidationResult& validation) {
  if (version != 2) {
    return GolfRoundDecodeStatus::RejectedVersion;
  }
  if (holes != 9 && holes != 18) {
    return GolfRoundDecodeStatus::RejectedHoleCount;
  }
  const uint8_t holeCount = static_cast<uint8_t>(holes);

  if (lengths.par != holeCount || lengths.putts != holeCount || lengths.in100 != holeCount ||
      lengths.out100 != holeCount || (lengths.expectYards && lengths.yards != holeCount)) {
    return GolfRoundDecodeStatus::RejectedArrayLength;
  }

  out.holeCount = holeCount;
  out.currentHole = currentHole < 0 || currentHole >= holes ? holeCount : static_cast<uint8_t>(currentHole);

  validation = validateGolfRound(out);
  if (!validation.valid) {
    return GolfRoundDecodeStatus::RejectedHoleCount;
  }
  return GolfRoundDecodeStatus::Ok;
}

#endif
