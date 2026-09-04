#pragma once

#include <cstdint>

#include "GolfRules.h"

// Positional field-cycle decision for the scoring screen (CONTRACTS-V2 §10).
// Deliberately free of GolfRound so it stays host-testable in isolation; the
// activity supplies the focused field and state for the explicit current player.

enum class GolfConfirmAction : uint8_t {
  CycleFocus,           // move focus to the next field, nothing else
  CommitAndAdvance,     // commit the displayed values and move to the next flattened turn
  AdvanceWithoutCommit  // move to the next turn without changing this player's score
};

// Confirm normally includes a configured short Power click. Scoring reserves
// Power for field cycling, so only a Confirm edge without the same-frame raw
// Power edge came from the remapped front button.
constexpr bool golfConfirmFromFrontButton(const bool confirmReleased, const bool powerReleased) {
  return confirmReleased && !powerReleased;
}

// Returns what the activity should do for one field-cycle press.
//   focusedField      - the currently focused scoring field
//   holeLogged        - the hole already has a score (a counter was mutated)
//   hasValuesToCommit - the displayed values are non-zero and worth storing
//                       (an unlogged hole pre-seeded to par)
// Putts and In100 cycle focus. Out100 always advances, committing only when this
// player/hole is unlogged and has displayed values to store. The caller uses
// advanceGolfTurn() for the player-then-hole transition.
GolfConfirmAction golfConfirmPress(GolfField focusedField, bool holeLogged, bool hasValuesToCommit);
