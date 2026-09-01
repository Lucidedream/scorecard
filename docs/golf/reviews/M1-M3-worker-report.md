# M1-M3 multiplayer UX worker report

## Outcome

Implemented the approved fixed golf header, positional two-cell totals band, and player-count-first setup flow on `qiliu4/multiplayer`. The existing `GolfPlayerSetupActivity` course-backed draft, `Players`/`TeeChoice` phases, tee resolution, persistence, and scoring transition were reused; `Count` is a new first phase in that same activity rather than a parallel activity.

The count UI adds no heap allocation. Its state is one `uint8_t` plus one `TeeSelection`, its pips and text render through the existing allocation-free FreeInkUI host, and count changes mutate the existing fixed-capacity `GolfRound` draft in place. No task was created, and every persistence call in `completeRound()` occurs after its `RenderLock` scope ends.

## Acceptance evidence

- Golf build: `.venv/bin/pio run -e golf` succeeded.
  - PlatformIO flash usage: 5,527,787 / 6,553,600 bytes = 84.3%.
  - Delta from the supplied 5,526,207 baseline on the same PlatformIO usage figure: +1,580 bytes.
  - Actual `.pio/build/golf/firmware.bin`: 5,541,504 bytes. Compared literally with 5,526,207, the delta is +15,297 bytes; the supplied baseline appears to be PlatformIO's application usage rather than the padded binary's filesystem size, so both figures are reported.
- Stock build: `.venv/bin/pio run -e default` succeeded.
  - `riscv32-esp-elf-nm -C .pio/build/default/firmware.elf | rg -i 'golf|scorecard'` returned only `ScorecardIcon`.
- Host tests: 209/209 golf tests passed: the original 204 plus five new tests for header sizing/distribution, both totals modes, solo skip/review policy, and count bounds.
- Formatting: hashes of the full diff before and after `PATH=/opt/homebrew/opt/llvm/bin:$PATH ./bin/clang-format-fix -g` were identical (`9602352...16969f`), and `git diff --check` passed.
- Touchpoints: 15 existing upstream files. The audit enumerated paths from `git diff upstream/develop --name-only`, excluded dedicated golf/docs/test directories, and retained only paths present in `upstream/develop`; this task added no sixteenth touchpoint.
- Header coupling: `rg -n 'metrics\.headerHeight' src/activities/golf` returned no matches.
- Default names are now `Noah`, `Player 2`, `Player 3`, and `Player 4`.
- The pre-existing modifications to `docs/golf/CONTRACTS-V2.md` and untracked `docs/golf/design/multiplayer-ux.html` were preserved.
- No commit was created.

## Scoring-screen vertical fit at 480 x 800

Measured with the layout's 15 px top padding, 24 px font minimum, penalty visible, and the middle counter focused. The available band height is 785 px in both cases.

| Band | Before, Lyra 84 px | After, fixed 46 px | Recovered allocation |
| --- | ---: | ---: | ---: |
| Header | 84 | 46 | -38 |
| Hole | 83 | 87 | +4 |
| Counter 1 | 116 | 123 | +7 |
| Focused counter 2 | 164 | 175 | +11 |
| Counter 3 | 116 | 123 | +7 |
| Penalty | 50 | 52 | +2 |
| Totals | 88 | 91 | +3 |
| Nine strip | 84 | 88 | +4 |
| Total | 785 | 785 | 0 |

The 38 px goes entirely to the positive-weight bands because the header weight is zero: 4 + 7 + 11 + 7 + 2 + 3 + 4 = 38 px.

## Hardware verification still required

On the X4, verify the count screen with side Up/Down and front Prev/Next, confirm count 1 goes directly to hole 1, confirm counts 2-4 open the roster, and press Back from the roster to tee off. Check both a par-bearing course (`Thru | To par`) and the par-free Template course (`Thru | Score`) in portrait and landscape orientations.
