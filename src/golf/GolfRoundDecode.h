#pragma once

#include <cstdint>

#include "GolfRound.h"
#include "GolfValidate.h"

// Pure checker shared by the state-file loader (GolfRoundStore::fromJson) and the
// completed-round reader (GolfRoundFile). It owns every decision that does not need a
// JSON tokenizer: the version gate, the hole-count check, the per-array
// length-equals-holes check, and the final validateGolfRound() pass. The per-hole
// arrays are read straight into `out` (and range-checked) by the shared JSON helper
// before this runs. Keeping this JSON-free is what makes it host-testable, as with
// GolfValidate / GolfCsv / GolfHistory / GolfConfirm.

enum class GolfRoundDecodeStatus : uint8_t {
  Ok,
  RejectedVersion,      // "v" is not 2 (a v1 file gets its own log line at the call site)
  RejectedHoleCount,    // holes is neither 9 nor 18
  RejectedArrayLength,  // a per-hole array length disagrees with holes
};

// Source lengths of the per-hole arrays, as decoded. A length that disagrees with
// holes is a corrupt file (CONTRACTS §5.4) and is rejected rather than padded or
// truncated. `yards` is ignored unless `expectYards` (the completed-round schema
// omits it).
struct GolfRoundColumnLengths {
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t yards;
  bool expectYards;
};

// Finalises a round whose per-hole arrays are already in `out` (written and
// range-checked by golfReadJsonHoleArray). The caller has also copied
// courseName / tees / dateYmd into `out`.
//   version     - raw "v"
//   holes       - raw "holes"
//   currentHole - raw "currentHole"; pass 0 for the completed-round schema, which omits it
// On Ok, `out.holeCount` / `out.currentHole` are set (currentHole clamped by
// validateGolfRound) and `validation` carries any repair for the caller to log.
GolfRoundDecodeStatus golfCheckRound(GolfRound& out, int version, int holes, int currentHole,
                                     const GolfRoundColumnLengths& lengths, GolfValidationResult& validation);
