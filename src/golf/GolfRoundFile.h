#pragma once

#include "GolfRound.h"

// Reads one completed-round file from /golf/rounds/<name>.json (CONTRACTS-V2 §8).
// The completed-round schema is the state schema minus currentHole and yards
// (CONTRACTS.md §5.2), so this shares GolfJson / GolfRoundDecode with
// GolfRoundStore::fromJson.
//
// Returns false — never a partial round — on a missing file, unparseable JSON, a
// rejected "v": 1 record, an array length that disagrees with "holes", or a
// validation rejection. A repair applied by validateGolfRound() is logged, not
// rejected. History selects one row and calls this once; on failure the caller
// falls back to the CSV-only summary screen.
bool loadGolfRoundFile(const char* path, GolfRound& out);
