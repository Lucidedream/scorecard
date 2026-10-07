# v7 X4 Pro worker report

## Result

The Scorecard app now has a dedicated `golf-x4pro` build and every concrete golf
activity has a touch-reachable control path. The build profile is defined at
`platformio.ini:176-194`. The shared touch footer and its constexpr geometry are at
`src/activities/golf/GolfUiLayout.h:81-173`; its runtime touch/non-touch branches are at
`src/activities/golf/GolfUiLayout.cpp:46-130`. The footer state is a small stack object
containing four pointers, two action enums, one reference, two coordinates, and one bool;
it performs no allocation. The body reservation is runtime-gated by `hasTouch()` in
`src/activities/golf/GolfUiLayout.cpp:18-24`, so the X4 keeps its prior content bounds.

The shared list adapter checks footer actions before `UiListActivity` routes row taps
(`src/activities/golf/GolfUiListActivity.cpp:11-45`). Scoring checks the footer first,
supports direct previous/next taps, and restricts counter-band taps to focus changes
(`src/activities/golf/GolfScoringActivity.cpp:581-657`). Its mark sheet applies a tapped
option and cancels a tap outside the sheet (`src/activities/golf/GolfScoringActivity.cpp:329-408`).
All touch calls used by golf are unconditionally declared in
`src/MappedInputManager.h:54-68`.

## X4 Pro activity reachability

The table covers every concrete `*Activity` in `src/activities/golf/`.
`GolfUiListActivity` is an abstract shared adapter rather than a screen.

| Activity | Every user action | X4 Pro input reaching it |
| --- | --- | --- |
| `GolfHomeActivity` | Leave golf; focus Resume/New Round/History/Course Maps/Tips; open the focused destination or resume the round | Back footer or left-edge Back swipe; Previous/Next footer or Up/Down side key; Confirm/Open/Start footer or direct resume/tile tap |
| `GolfSetupActivity` | Leave; move course selection; choose course | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer |
| `GolfPlayerSetupActivity` | Leave/back one setup phase; change player count; advance from count; select a player or Start; edit player name; select tee | Back footer/swipe; Previous/Next footer or Up/Down side key; Confirm/Next footer; direct enabled-row tap or Confirm/Select footer. The launched keyboard supplies touch name entry |
| `GolfScoringActivity` | Open round menu; open course map; previous/next turn; focus Out 100/In 100/Putts; increment; decrement/remove latest mark; cycle field/advance; open mark sheet; move/apply/cancel mark choice | MENU footer or Back swipe; long-press MENU footer; PREV/NEXT footer; counter-band tap; Up side key; Down side key; Power or FIELD footer according to short-Power setting; MARK footer or long-press FIELD footer; Up/Down plus Confirm/Power, direct option tap, Back footer, or tap outside sheet |
| `GolfRoundMenuActivity` | Leave; move selection; View Card, Abandon Round, Finish Round, Send, or Tips | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer; confirmation dialog handles destructive choices |
| `GolfCardActivity` | Leave; previous/next nine-hole tab | Back footer/swipe; Previous/Next Tab footer, Up/Down side key, or direct tab tap |
| `GolfCourseMapActivity` | Dismiss map or missing-map message | Back footer or Back swipe |
| `GolfCourseMapListActivity` | Leave; move course selection; open a course map browser | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer |
| `GolfCourseMapBrowserActivity` | Leave; previous/next hole; open tee information | Back footer/swipe; Previous/Next footer or Up/Down side key; Tee footer |
| `GolfCourseTeeInfoActivity` | Leave; previous/next tee tab | Back footer/swipe; Previous/Next footer, Up/Down side key, or direct tab tap |
| `GolfPlayerSelectActivity` | Leave; move player selection; open selected player's history choices | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Open footer |
| `GolfHistoryChoiceActivity` | Leave; focus Trends/Statistics/Rounds; open focused choice | Back footer/swipe; Previous/Next footer or Up/Down side key; direct tile tap or Confirm/Open footer |
| `GolfHistoryActivity` | Leave; move round selection; open selected round | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer |
| `GolfHistoryRoundMenuActivity` | Leave; move selection; open Scorecard/Hole by Hole/Statistics/Send; delete round | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer; confirmation dialog handles deletion |
| `GolfHoleReviewActivity` | Leave; previous/next hole | Back footer/swipe; Previous/Next footer or Up/Down side key |
| `GolfStatisticsActivity` | Dismiss | Back footer, Back swipe, physical Confirm if available, or tap anywhere in the body |
| `GolfCareerStatsActivity` | Dismiss; page through overflow statistics | Back footer/swipe or physical Confirm if available; More footer or Up/Down side key |
| `GolfTrendsActivity` | Dismiss | Back footer, Back swipe, or physical Confirm if available |
| `GolfRoundSummaryActivity` | Leave; Send archived round or Share summary | Back footer/swipe; Send/Share footer |
| `GolfRoundExportActivity` | Finish export view; switch between QR and report | Done footer/swipe; Next/Previous footer |
| `GolfTipListActivity` | Leave; move note selection; open selected note | Back footer/swipe; Previous/Next footer or Up/Down side key (hold pages); direct row tap or Confirm/Select footer |
| `GolfTipNoteActivity` | Leave; previous/next section | Back footer/swipe; Previous/Next footer or Up/Down side key |
| `GolfMessageActivity` | Dismiss | Back footer, Back swipe, physical Confirm if available, or tap anywhere in the body |

## Power click and double-click finding

The main loop records the first short Power release, toggles and persists the frontlight
on the second release, and consumes that second release before activity dispatch
(`src/main.cpp:214-236`, `src/main.cpp:689-703`). With `PWR_CONFIRM`, the first click is
delayed until the double-click window expires and then injected as Confirm; Scoring sees
that one delayed event as its FIELD action and cycles/advances once. A double-click with
`PWR_CONFIRM` toggles the frontlight and cycles the scoring field **zero** times: the first
raw Power release is excluded from the Confirm path and the second release clears the
pending click before returning from the main loop.

With another non-Sleep short-Power setting, Scoring reserves raw Power for FIELD, so a
single click cycles/advances immediately. A double-click in that configuration toggles
the frontlight and cycles/advances **once**, from its first release; its consumed second
release cannot cycle a second time. With `SLEEP`, Scoring does not bind Power to FIELD and
the firmware sleep behavior remains authoritative. The relevant Scoring partition and
same-frame raw-Power exclusion are at `src/activities/golf/GolfScoringActivity.cpp:607-636`.

## Verification

All commands were run from the repository root on macOS (`Darwin`).

| Command | Result |
| --- | --- |
| `~/.platformio/penv/bin/pio run -e golf-x4pro` | PASS. Flash 5,510,658 / 6,553,600 bytes (84.1%); reported RAM 90,628 / 327,680 bytes (27.7%); image 5,511,006 bytes. Only the pre-existing WebSockets deprecated `flush()` warning appeared. |
| `~/.platformio/penv/bin/pio run -e golf` | PASS. Flash 5,619,787 / 6,553,600 bytes (85.8%); reported RAM 57,204 / 327,680 bytes (17.5%); image 5,633,339 bytes. No new warning was observed. |
| `cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j` | PASS: 538/538 tests. The cached host configuration uses Homebrew LLVM because the active Apple Command Line Tools installation lacks the C++ standard-library headers; googletest was already populated locally after the initial network fetch failed. |
| `./bin/clang-format-fix -g` | PASS; completed without output, followed by a clean `git diff --check`. |
| `git diff --stat main... -- . ':!src/activities/golf' ':!src/golf' ':!test' ':!docs'` | Expected result after commits: `platformio.ini` only. |

The new host tests cover footer tiling, all four cell-centre round trips, misses above the
footer and in each bezel direction, and empty-label inertness in
`test/golf_layout/test_golf_layout.cpp:290-329`.

## Not verified here

No X4 Pro hardware was available. Device-owner verification remains: flash the Pro,
corner-tap every footer cell, score three holes with side keys and Power, open both the
mark sheet and course map, finish/export a round, inspect Scoring and Card at the 1.2 UI
scale for clipping, and confirm RTC-backed dates without Wi-Fi sync. The builds establish
compile/link compatibility and static memory totals, but do not prove touch calibration,
e-ink appearance/refresh behavior, frontlight timing on a physical switch, RTC accuracy,
or runtime heap high-water behavior. On-device verification should monitor the serial log
and `ESP.getFreeHeap()` before entering golf, during Scoring/mark-sheet use, and after
returning Home to confirm no lifecycle leak.

## Review fix

A review found that the first touch-footer rewrite accidentally changed the open mark
sheet from Confirm-or-Power to Confirm-without-Power. The picker now applies its highlighted
row when either the footer/physical Confirm release or a bare Power release occurs
(`src/activities/golf/GolfScoringActivity.cpp:361-363`), restoring the X4 behavior and the
X4 Pro's physical confirmation path. A pair-by-pair audit of every other input-condition
replacement in `git diff main...HEAD -- src/activities/golf` found no further semantic
regressions: physical Back/Confirm conditions remain ORed with touch footer actions,
physical Left/Right direction swapping is preserved, touch Previous/Next remains direct,
list side-key navigation still falls through to `ButtonNavigator`, and the two main
Scoring call sites still deliberately exclude same-frame Power from front Confirm.
After the fix, `pio run -e golf-x4pro` passed at 5,510,650 bytes of flash and
90,628 bytes of reported RAM, `pio run -e golf` passed at 5,619,785 bytes of flash
and 57,204 bytes of reported RAM, and the host suite again passed all 538 tests.
