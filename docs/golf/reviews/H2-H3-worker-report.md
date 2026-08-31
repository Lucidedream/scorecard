# H2+H3 history redesign worker report

## Result

Implemented the approved archived-round history flow: a round hub, a three-row scorecard, a structurally read-only hole review, and a dedicated statistics page. The implementation follows the coordinator-provided 1:1 mock in `docs/golf/design/history-redesign.html`; the contract remains authoritative.

History now sends a successfully decoded round to the new hub and retains `GolfRoundSummaryActivity` as the unreadable/missing-JSON fallback (`src/activities/golf/GolfHistoryActivity.cpp:68`, `src/activities/golf/GolfHistoryActivity.cpp:93`). The hub exposes Scorecard, Hole by hole, Statistics, and Delete round as four normal rows, formats the score/to-par status and two-line round information band, and reuses `ConfirmationActivity` plus `RoundArchive::remove()` (`src/activities/golf/GolfHistoryRoundMenuActivity.cpp:29`, `src/activities/golf/GolfHistoryRoundMenuActivity.cpp:45`, `src/activities/golf/GolfHistoryRoundMenuActivity.cpp:77`, `src/activities/golf/GolfHistoryRoundMenuActivity.cpp:105`).

The card now has only Front 9 and Back 9 tabs and renders Hole/Par/Score, with Par omitted on a par-free course (`src/activities/golf/GolfCardActivity.cpp:99`, `src/activities/golf/GolfCardActivity.cpp:123`). Its explicit row heights are 40/44/56 px, so Score is the largest row (`src/activities/golf/GolfCardActivity.h:24`). Penalty holes receive a small H or O marker (`src/activities/golf/GolfCardActivity.cpp:194`), and first paint remains `HALF_REFRESH` (`src/activities/golf/GolfCardActivity.cpp:214`).

The hole review walks and wraps holes with mapped Left/Right input, renders par/yards/SI, a 110 px-high score figure with conditional to-par, the three detail fields in entry order, field-local penalty markers, and a conditional penalty band (`src/activities/golf/GolfHoleReviewActivity.cpp:27`, `src/activities/golf/GolfHoleReviewActivity.cpp:34`, `src/activities/golf/GolfHoleReviewActivity.cpp:54`, `src/activities/golf/GolfHoleReviewActivity.cpp:79`, `src/activities/golf/GolfHoleReviewActivity.cpp:92`, `src/activities/golf/GolfHoleReviewActivity.cpp:127`). Its first paint is also `HALF_REFRESH` (`src/activities/golf/GolfHoleReviewActivity.cpp:145`).

The statistics page obtains counts from `GolfStats` and `GolfPenalty`: long game, short game, putting, penalty strokes, one-putts, three-putts, hazards, and out of bounds (`src/activities/golf/GolfStatisticsActivity.cpp:60`). The four shot shares use the round score as their common denominator (`src/activities/golf/GolfStatisticsActivity.cpp:35`). Common round-status formatting suppresses to-par when `golfHasPar()` is false (`src/activities/golf/GolfReviewFormat.cpp:17`).

All four screens use `mappedInput.mapLabels()` and `GUI.drawButtonHints()` for their footers (`src/activities/golf/GolfHistoryRoundMenuActivity.cpp:139`, `src/activities/golf/GolfCardActivity.cpp:208`, `src/activities/golf/GolfHoleReviewActivity.cpp:139`, `src/activities/golf/GolfStatisticsActivity.cpp:54`).

## Memory and ownership

No task is created and there is no render-time heap allocation in the new screens. Child screens use the repository's existing activity ownership pattern with `makeUniqueNoThrow` and an OOM log/check before navigation (`src/activities/golf/GolfHistoryRoundMenuActivity.cpp:45`). Each review activity owns one inline `GolfRound` snapshot so the archived record remains valid independently of the parent activity and exposes no mutable store; the object itself is allocated once by the activity framework.

Reducing the card's cell store from `7 * 12 * 16` bytes to `3 * 12 * 8` bytes lowers that activity member from 1,344 bytes to 288 bytes, a direct 1,056-byte heap reduction because activities are heap allocated (`src/activities/golf/GolfCardActivity.h:24`). The shared large-number renderer moves the existing seven-segment code out of `GolfScoringActivity` rather than adding a second copy.

## Structural read-only check

The hole review contains zero references to mutation or persistence entry points. Checked with:

```sh
rg -n '(incrementGolfCounter|decrementGolfCounter|golfAppendPenalty|golfRemoveLatestPenalty|seedGolfHoleAtPar|markGolfRoundDirty|flushGolfRound|GOLF_ROUND_STORE|getRound\(\))' \
  src/activities/golf/GolfHoleReviewActivity.*
```

The command returned no matches. The activity stores a private copied `GolfRound` and only mutates its local `currentHole` navigation index (`src/activities/golf/GolfHoleReviewActivity.h:19`, `src/activities/golf/GolfHoleReviewActivity.cpp:34`).

## 800 px vertical fit

The default theme is Lyra (`src/CrossPointSettings.h:280`). Lyra reserves 5 px top padding, 84 px header, and 40 px footer, leaving `800 - 5 - 84 - 40 = 671 px` for content (`src/components/themes/lyra/LyraTheme.h:9`).

| Screen | Content calculation | Content height | Full height | Spare |
| --- | ---: | ---: | ---: | ---: |
| Round menu | `4 * 82 + 70` | 398 px | 527 px | 273 px |
| Card, one nine | `40 tab + 16 gap + 40 + 44 + 56` | 196 px | 325 px | 475 px |
| Hole review, no penalty | `120 + 180 + 3 * 76` | 528 px | 657 px | 143 px |
| Hole review, penalty | previous `+ 60` | 588 px | 717 px | 83 px |
| Statistics | `3 * 34 + 8 * 62` | 598 px | 727 px | 73 px |

For a possible whole-round card, a conservative stacked design needs two blocks of `40 px nine-label + 40 + 44 + 56 px rows`, plus a 16 px inter-block gap: `2 * 180 + 16 = 376 px`. That fits the 671 px content region with 295 px spare. Recommendation: both nines technically fit comfortably, so the owner should trial a stacked no-tabs version on the physical panel; tabs were retained in this change as explicitly required pending that decision.

## Verification

- `.venv/bin/pio run -e golf`: succeeded after the final source edit. PlatformIO reports 5,483,965 / 6,553,600 bytes of linked flash, 83.7%, and 59,604 / 327,680 bytes RAM, 18.2%.
- `.pio/build/golf/firmware.bin`: 5,497,680 bytes, +5,488 bytes from the supplied 5,492,192-byte baseline. Raw binary size is 83.9% of 6,553,600; this is distinct from PlatformIO's 83.7% linked-flash report.
- `.venv/bin/pio run -e default`: succeeded. `nm -C .pio/build/default/firmware.elf | rg -i 'golf|scorecard'` reports only `ScorecardIcon`.
- Host tests: all 296 repository tests passed; the exact Golf/golf filter enumerates and passes all 128 golf tests.
- `PATH=/opt/homebrew/opt/llvm/bin:$PATH ./bin/clang-format-fix -g`: a before/after SHA-1 comparison over modified/untracked C/C++ files is identical, so the formatter is idempotent. `git diff --check` is clean.
- Upstream touchpoints remain exactly six: `platformio.ini`, `src/activities/home/HomeActivity.cpp`, `src/components/themes/BaseTheme.h`, `src/components/themes/lyra/LyraTheme.cpp`, `src/main.cpp`, and `test/CMakeLists.txt`. This task added no upstream touchpoint.

Physical-device rendering and interaction remain for the owner's panel acceptance.
