# M3 — Player-count screen & solo roster skip: completion report

## Summary

The count screen, the solo-skip flow, and the `{"Noah","Player 2","Player 3","Player 4"}`
rename were **already present in the working tree** from the prior worker (they survived
in `GolfPlayerSetupActivity` + the untracked `GolfPlayerSetupPolicy.h`, only the terminal
was lost). Verification confirmed all of it builds and behaves per §16.3. The one real
gap was the required **pure-function coverage of the Confirm label**: the label decision
lived inline as `playerCount == 1` in `drawFooter()` with no test. That is now a pure
function with a test.

## What I changed (M3 close-out only)

| File | Change |
| --- | --- |
| `src/activities/golf/GolfPlayerSetupPolicy.h` | Added `enum class GolfCountConfirmLabel { Start, Next }` and `constexpr golfCountConfirmLabel(count)`, derived from the existing `golfPlayerSetupNext()` so the label and the navigation target have **one owner**. |
| `src/activities/golf/GolfPlayerSetupActivity.cpp` | `drawFooter()` Count branch now calls `golfCountConfirmLabel(playerCount)` instead of the inline `playerCount == 1`. Behaviour identical; constexpr-folds to the same code (firmware byte-identical). |
| `test/golf_layout/test_golf_layout.cpp` | New `GolfPlayerSetupPolicy.ConfirmLabelIsStartAtOnePlayerAndNextAbove`; extended `CountStepsStayWithinOneAndFour` with `golfClampPlayerCount(0)==1` and `golfClampPlayerCount(5)==4`. |

No new files, no touchpoints, no heap, no tasks.

## What the prior worker built and I reused (verified, not modified)

- **`Phase::Count` as the first phase** of `GolfPlayerSetupActivity` — extends the existing
  `Players` / `TeeChoice` state machine rather than a parallel activity. `onEnter()` sets
  `phase = Phase::Count`, `playerCount = 1`.
- **Both input habits:** `navigateButtons()` Count branch wires `onNext/PreviousRelease`
  and `onNext/PreviousContinuous` to `stepPlayerCount(±1)`. `ButtonNavigator` routes these
  through `MappedInputManager::Button::NavNext` / `NavPrevious`, each of which is
  `side Down/Up  OR  front Right/Left` (`src/MappedInputManager.cpp:111-120`). Side rocker
  and front Prev/Next both step.
- **Clamp:** `golfStepPlayerCount()` / `golfClampPlayerCount()` in the policy header, range
  `1..GOLF_MAX_PLAYERS` (4), no wrap past either end.
- **Pips, not a sentence:** `buildScreen()` Count branch draws `GOLF_MAX_PLAYERS` pip
  rects (filled ≤ count, stroked above); no "maximum four" text.
- **Footer:** `mappedInput.mapLabels(...)` + `GUI.drawButtonHints(...)` only — no
  hand-drawn geometry (§15.3).
- **Confirm label:** `Start` at count 1, `Next` at 2+ (now via `golfCountConfirmLabel`).
- **Roster skip / Back-tees-off:** see below.
- Default names already `{"Noah","Player 2","Player 3","Player 4"}` in `GolfRound.h`.

## Acceptance evidence

| # | Criterion | Result |
| --- | --- | --- |
| 1 | `.venv/bin/pio run -e golf` | **SUCCESS.** `firmware.bin` = **5,541,504 bytes**, **delta 0** from the 5,541,504 baseline. Flash **84.3%** (5,527,787 / 6,553,600 B application). RAM 17.4%. |
| 2 | `.venv/bin/pio run -e default` | **SUCCESS.** `nm -C firmware.elf | grep -i 'golf\|scorecard'` → only `ScorecardIcon`. No golf symbols. |
| 3 | Host tests | **210/210 golf pass** (209 prior + new `ConfirmLabelIsStartAtOnePlayerAndNextAbove`). Full suite **378/378**. Count/skip coverage: `CountOwnsRosterAndSoloSkipsReview` (count 1 → `StartRound`, counts 2–4 → `ReviewRoster`; `golfApplyPlayerCount` tee enable/disable; renumbered names), `CountStepsStayWithinOneAndFour` (clamp at both ends, no wrap), `ConfirmLabelIsStartAtOnePlayerAndNextAbove`. |
| 4 | `clang-format-fix -g` | Clean — exits 0 with no output, `git diff --check` clean. (First run wrapped one long line in `drawFooter`; idempotent thereafter.) |
| 5 | Touchpoints | **Unchanged.** Strict audit (`git diff upstream/develop`, existing-in-upstream, excluding `src/activities/golf/`, `src/golf/`, `test/`, `docs/`) → 14 files, none added by M3. My edits touched only golf-namespaced + test files. If the canonical count is 15 it remains 15. |
| 6 | How count 1 avoids rendering the roster | See below. |
| 7 | Count-screen vertical fit at 800 px | See below. |

## #6 — A count of 1 never renders the roster at all

`handleButtons()` in the Count phase, on Confirm release:

```cpp
if (golfPlayerSetupNext(playerCount) == GolfPlayerSetupNext::StartRound) {
  completeRound();      // count == 1
} else {
  showPlayers();        // count >= 2 — the ONLY setter of Phase::Players from Count
}
```

At count 1 `completeRound()` runs: it resolves tees on the in-place fixed-capacity
`draft`, sets `draft.currentHole = 0`, persists via `GOLF_ROUND_STORE`, then calls
`openGolfScoring(...)` which navigates straight to the scoring activity — at which point
`main.cpp` deletes `GolfPlayerSetupActivity`.

`phase` is never assigned `Phase::Players`. Therefore `listCount()` stays `0`,
`buildScreen()` only ever executes its `phase == Phase::Count` branch and returns before
`screen.list(listProps)`, and `refreshPlayerRows()`'s output is never presented. The
roster is not drawn-then-skipped — its render path is never entered. `onBackButton()` at
Count goes to `openGolfSetup` (course screen), not the roster.

At 2+, `showPlayers()` sets `Phase::Players`; `onBackButton()` there calls
`completeRound()` — Back tees off, it does not cancel (§16.3).

## #7 — Count-screen vertical fit at 800 px

`buildScreen()` centres a fixed 5-element block in `screen.body()`:

```
blockHeight = questionH + valueH + unitH + PIP_SIZE(15) + 3*BLOCK_GAP(18)   // 54 px of gaps
y           = body.y + (body.height - blockHeight) / 2
```

The text heights are `screen.target().lineHeight()` of the active theme's `titleText`
(question + numeral) and `bodyText` (unit) — the golf UI fonts, ~28 px and ~24 px
respectively. So `blockHeight ≈ 28 + 28 + 24 + 15 + 54 ≈ 149 px`.

Content band at 480×800 portrait: safe height 800 − 7 − 11 (bezel) = 782; minus header
46, minus top padding 15, minus the hint-edge reserve (~48) ⇒ **body ≈ 673 px**.
Centred, the block leaves **≈ 262 px clear above and below** — no clipping, large margin.
It also clears the 800×480 landscape case (body ≈ 300 px, ≈ 75 px each side). The
`(body.height - blockHeight)/2` offset only goes negative below ~150 px of body height,
which neither orientation approaches.

## Observations for the owner (not blockers, out of M3 scope)

1. **Numeral size.** The mock's defining visual is a ~190 px numeral; the implementation
   renders the count at `titleText` size (~28 px). Functionally correct and it fits, but
   it does not carry the mock's visual weight. Bumping it needs a large font id registered
   for the golf build.
2. **Footer glyphs.** Mock shows `−` / `+` for the step cells; implementation shows
   `Prev` / `Next` text. §16.3 doesn't mandate the glyphs; align with the design if desired.
3. Hardware pass still required: walk count 1 → hole 1 (no roster flash), counts 2–4 →
   roster, Back from roster tees off, both side rocker and front Prev/Next step the count,
   in portrait and both landscape orientations.

## Not done

- No commit (per instructions).
- i18n generated files regenerated locally for the build; only `english.yaml` is tracked
  and it already carried `STR_GOLF_HOW_MANY_PLAYERS/PLAYERS/START/NEXT`.
