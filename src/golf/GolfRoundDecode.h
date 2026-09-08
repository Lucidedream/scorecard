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
  // Wire length of the v5 "fairways" array; 0 for a v2/v3/v4 record (not checked).
  uint16_t fairways;
  // Wire length of the v6 "bunkers" array; 0 for a pre-v6 record (not checked).
  uint16_t bunkers;
};

struct GolfRoundColumnLengths {
  uint16_t par;
  uint16_t si;
  uint16_t players;
  GolfPlayerColumnLengths player[GolfRound::MAX_PLAYERS];
  bool expectLegacyYards;
};

// True when `s` is a well-formed free-form tee name (CONTRACTS-V2 §32.1):
// 1..GOLF_TEE_CAPACITY-1 bytes, no ',' '\r' '\n', valid UTF-8. The empty string
// (the "did not play" sentinel) is not "valid" here -- callers test it directly.
bool golfTeeStringValid(const char* s);

// Maps a pre-v4 doc-level "tees" label to a tee name string. Intentionally falls
// back to "Blue" for every non-"White" old label.
const char* golfLegacyTeeSelection(const char* legacyTee);
void golfInitializeLegacyRound(GolfRound& round, const char* legacyTee);

GolfRoundDecodeStatus golfCheckRound(GolfRound& out, int version, int holes, int currentHole, int currentPlayer,
                                     const GolfRoundColumnLengths& lengths, GolfValidationResult& validation);
