#pragma once

#include <cstdint>

#include "GolfRules.h"

// Positional field-cycle decision for the scoring screen (CONTRACTS-V2 §10).
// Deliberately free of GolfRound so it stays host-testable in isolation; the
// activity supplies the focused field and the two hole-state booleans.

enum class GolfConfirmAction : uint8_t {
  CycleFocus,           // move focus to the next field, nothing else
  CommitAndAdvance,     // commit the displayed values and move to the next hole
  AdvanceWithoutCommit  // move to the next hole without changing its score
};

// Returns what the activity should do for one field-cycle press.
//   focusedField      - the currently focused scoring field
//   holeLogged        - the hole already has a score (a counter was mutated)
//   hasValuesToCommit - the displayed values are non-zero and worth storing
//                       (an unlogged hole pre-seeded to par)
// Putts and In100 cycle focus. Out100 always advances, committing only when the
// hole is unlogged and has displayed values to store.
GolfConfirmAction golfConfirmPress(GolfField focusedField, bool holeLogged, bool hasValuesToCommit);
