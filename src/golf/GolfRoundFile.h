#pragma once

#include "GolfRound.h"

struct GolfRoundFileInfo {
  bool penaltiesRecorded = true;
  bool repaired = false;
};

// Reads one completed group-round file from /golf/rounds/<name>.json. V4 uses
// the same shared/player payload as state.json and omits only currentHole and
// currentPlayer. V2/V3 are migrated into slot 0 while v1 remains rejected.
//
// Returns false without modifying `out` on a missing file, malformed JSON,
// unsupported version, invalid fixed-array length, OOM, or validation failure.
bool loadGolfRoundFile(const char* path, GolfRound& out, GolfRoundFileInfo* info = nullptr);
