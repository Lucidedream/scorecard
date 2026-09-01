#pragma once

#include <cstdint>

#include "GolfRound.h"
#include "GolfValidate.h"

enum class GolfRoundDecodeStatus : uint8_t {
  Ok,
  RejectedVersion,
  RejectedHoleCount,
  RejectedMetadata,
  RejectedPlayerCount,
  RejectedArrayLength,
  RejectedDisabledPlayerData,
  RejectedSharedData,
  RejectedRound,
};

// Actual wire lengths captured while decoding. V4 validates all four player
// records; v2/v3 use only player[0] and optionally omit archived yards.
struct GolfPlayerColumnLengths {
  uint16_t yards;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t penalties;
};

struct GolfRoundColumnLengths {
  uint16_t par;
  uint16_t si;
  uint16_t players;
  GolfPlayerColumnLengths player[GolfRound::MAX_PLAYERS];
  bool expectLegacyYards;
};

// Canonical v4 tokens are language-independent and case-sensitive. The legacy
// mapper intentionally falls back to Blue for every non-exact old label.
const char* golfTeeSelectionToken(TeeSelection tee);
bool golfParseTeeSelection(const char* token, TeeSelection& tee);
TeeSelection golfLegacyTeeSelection(const char* legacyTee);
void golfInitializeLegacyRound(GolfRound& round, const char* legacyTee);

GolfRoundDecodeStatus golfCheckRound(GolfRound& out, int version, int holes, int currentHole, int currentPlayer,
                                     const GolfRoundColumnLengths& lengths, GolfValidationResult& validation);
