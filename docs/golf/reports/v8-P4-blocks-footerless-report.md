# v8 P4 — blocks and footerless touch

Implemented CONTRACTS-V2 §43–§44, including the coordinator's written clarifications now recorded in §44. No push or flash was performed. Host checks establish geometry and policy; device interaction, e-ink appearance, and runtime heap remain hardware checks.

## Changes and evidence

- Shared `GolfTiles` draws the menu/History tiles and detail box, the picker tiles, region blocks, and course cards. The X4 menu keeps the same 112-pixel strip, 32-pixel icons, label rectangles, detail padding, border, text styles, and Resume/quote placement. `GolfBlocks.X4MainMenuMatchesPreExtractionRects` compares the extracted geometry with the original formulas across portrait/landscape dimensions, all four hint edges, three spacings, and three/four tiles. This is the requested layout-rectangle equivalence check, rather than a framebuffer or physical-device comparison.
- Hub tiles show counts and focused details, with four Recent previews. Recent and region availability gates and the single-destination bypass remain. Region entries sort by count descending, then name, retaining their original index/range. Regions use ten blocks per page and pips. Cards use a 104-pixel height, large rating/par figures, reviews, and map availability; the region sort segment selects Rating/Name. Course info exposes only action tiles, with rating, category bars, nines, and optional designer/year below.
- `GolfUiLayout` owns the touch footer policy in drawing, input, and safe-body layout. Touch defaults to no footer; Scoring and its in-place picker explicitly opt in. Non-touch still uses the original theme hints and physical hint clearance. `GolfFooterless.ChromeUsesFullTouchBodyAndRetainsX4HintsOnEveryEdge` tests this policy for every hint edge and both screen dimensions.
- Paging uses the existing public `MappedInputManager::wasBackGesture()`, which delegates directly to private `wasEdgeSwipe(ScreenEdge::Left)` (`src/MappedInputManager.cpp:284`). The pure paging helper rejects that event. Hole review, Tip note, Round export, Career stats, region paging, and Scoring's hole/strip swipes exclude edge Back.
- Existing return routing is retained through `GolfPickerContext`/`GolfSetupReturn`; Recent has a level of its own so returning from setup restores its selected card. The existing P3 return-context tests pass.

## Coordinator decisions

These decisions were requested through blocking Orca questions and incorporated into the contract:

1. Round export keeps Join/Report, toggled by horizontal swipe; left-edge Back is Done. No Open/Done body block.
2. Scoring's mark/fairway/GIR overlays retain their existing relabelled footer.
3. Touch player count has four rows, 1–4. A tap sets count and continues to Players. The roster ends with Start round. The X4 counter is unchanged.
4. Career stats More gets horizontal paging as well as its existing side buttons.
5. Shared confirmation dialogs and the name keyboard remain unchanged and touch-native. `BaseTheme.cpp:170` suppresses their button hints on touch; no upstream screen changes were needed.

## Complete X4 Pro reachability table

Every row below also has host Back through the left-edge gesture; Home uses the host Home key (`ActivityManager.cpp:93`, `MappedInputManager.cpp:299,315,325`). Error/loading/empty states retain this exit. “Inspection” means source-path verification, not a claim of hardware testing. The common list routes a row tap through `UiListActivity::onRowAction`, scrolls with vertical swipes, and supports side Up/Down; tile/card buttons use the same published FreeInkUI interaction table.

| Screen / state | Every action and X4 Pro access | Verification |
|---|---|---|
| Main menu | New round, History, Course Map, Tips: tap respective tile; Resume: tap Resume block when present; focus navigation: side buttons; Back/Home: host | Inspection `GolfHomeActivity.cpp:253,306`; extracted tile geometry test; both golf builds |
| Picker hub, Play and Browse | Recent, My courses, By region: tap visible tile; focus: side buttons; Open: tile tap; Back: host; one visible destination bypasses hub | Inspection `GolfCoursePickerActivity.cpp:18,261,416`; tile hit-boundary tests |
| Regions | Open region: tap block; previous/next page: horizontal swipe; previous/next block: side buttons; Back: host | Inspection picker `handleCustomInput`/`buildScreen`; grid boundary, count-order, page-count, edge exclusion tests |
| Region courses | Start/Open course: tap card; set Rating or Name: tap segment half on first window; focus/card selection: side buttons; scroll cards: vertical swipe; Back to regions: host | Inspection picker `buildScreen`/`activateIndex`; card and segment rectangle tests; catalog ordinal-sort tests |
| Recent cards | Start/Open: tap card; scroll: vertical swipe; focus: side buttons; Back to hub: host | Inspection picker `fillRow`/`activateIndex`; card boundary and existing return-context tests |
| My courses — Play (`GolfSetupActivity`) | Start: tap course card; scroll: vertical swipe; focus: side buttons; Back to picker parent: host | Inspection `GolfSetupActivity.cpp:95,105`; card layout tests; existing course-order/return tests |
| My courses — Browse (`GolfCourseMapListActivity`) | Open info: tap course card; scroll/focus: vertical swipe/side buttons; Back: host | Inspection `GolfCourseMapListActivity.cpp:95,111`; card layout and existing return-context tests |
| Course info | Play when allowed: tap Play tile; browse maps: tap Hole maps; tees: tap Tee info; focus: side buttons; facts are read-only; Back: host | Inspection `GolfCourseInfoActivity.cpp:86,156`; tile boundaries and existing Play-visibility tests |
| Tee info | Select nine: tap tab; previous/next tab: side buttons; Back: host | Inspection `GolfCourseTeeInfoActivity.cpp:59,99`; existing card/layout tests |
| Course-map browser | Previous/next hole: horizontal swipe or side buttons; Tee info: tap hole band; Back: host | Inspection `GolfCourseMapBrowserActivity.cpp:62`; shared edge-exclusion test |
| Nine picker — summary rows | Front/Back nine: tap corresponding row; Swap nines: tap row; Continue: tap row; scroll/focus: swipe/side buttons; Back: host | Inspection `GolfNineSelectActivity.cpp:85,151`; existing nine-picker/selection tests |
| Nine picker — choice list | Choose nine: tap row; scroll/focus: swipe/side buttons; Back returns to summary rows | Same source and tests as nine summary |
| Player setup — Count | Set 1–4 players and continue: tap number row; side buttons adjust count; physical Confirm still continues; Back: host | Inspection `GolfPlayerSetupActivity.cpp:149,438`; existing player-count policy tests; golf builds |
| Player setup — roster | Edit player/tee: tap player row; Start round: tap final row; scroll/focus: vertical swipe/side buttons; Back returns to Count | Inspection setup `refreshPlayerRows`/`activateIndex`; existing player setup tests |
| Player setup — tee/name choice | Name: tap name row to open keyboard; tee: tap tee row; scrolling/focus: vertical swipe/side buttons; Back returns to roster | Inspection setup `initializeTeeRows`/`activateIndex`; existing tee resolution tests |
| Scoring | MENU: footer tap; MARK/field control: tap or hold the Confirm footer according to the existing Power setting; previous/next turn: footer cells; focus field: tap counter; increment/decrement: tap counter thirds or side controls; jump hole: tap nine-strip cell; page strip/hole: horizontal swipe in band; map: hold MENU; Back opens round menu; Home: host | Inspection `GolfScoringActivity.cpp:590,640,1021`; existing counter-zone, scoring, strip and confirmation tests; footer policy/edge tests |
| Scoring — mark/fairway/GIR picker | Select/toggle and apply outcome: tap picker row; apply selected row: existing Confirm footer; cancel: existing Back footer or outside-sheet tap; side buttons move selection | Inspection `GolfScoringActivity.cpp:346`; footer opt-in and existing mark policy tests |
| Round menu | View card, Hole map, Send, Tips, Abandon, Finish: tap row; scroll/focus: vertical swipe/side buttons; destructive actions open confirmation; Back: host | Inspection `GolfRoundMenuActivity.cpp:34,125`; existing round-menu row tests |
| Scorecard (live and archived) | Select OUT/IN/TOTAL tab: tap tab; previous/next tab: side buttons; read-only table; Back: host | Inspection `GolfCardActivity.cpp:97,122`; existing card layout tests |
| Live round hole map (`GolfCourseMapActivity`) | Read current hole map; Back: host | Inspection `GolfCourseMapActivity.cpp:46`; shared footer policy tests |
| Tips list (from Home or round menu) | Open note: tap row; scroll/focus: vertical swipe/side buttons; Back: host | Inspection `GolfTipListActivity.cpp:38,50`; existing tips tests |
| Tip note | Previous/next section: horizontal swipe or side buttons; Back: host | Inspection `GolfTipNoteActivity.cpp:94`; shared edge-exclusion and existing section tests |
| Round summary | Send/share summary: tap body block; Back returns to Home or parent according to entry context; Home: host | Inspection `GolfRoundSummaryActivity.cpp:26,41,81`; published FreeInkUI action handler; existing review-format tests |
| Round export — Join and Report | Toggle page: horizontal swipe in either direction, matching two-page Confirm toggle; Done: left-edge Back; Home: host; automatic station/status transitions unchanged | Inspection `GolfRoundExportActivity.cpp:147`; shared edge-exclusion and existing export tests |
| Round export — busy/error/timeout | Done: left-edge Back; Home: host | Same source; error gate prevents swipe toggle |
| History — player selector | Choose player: tap row; scroll/focus: vertical swipe/side buttons; Back: host | Inspection `GolfPlayerSelectActivity.cpp:104,125`; existing player-select tests |
| History chooser | Trends, Stats, Rounds: tap tile; focus: side buttons; Back: host | Inspection `GolfHistoryChoiceActivity.cpp:118,157,199`; shared tile geometry tests |
| History — rounds list | Open archived round or CSV summary: tap row; scroll/focus: vertical swipe/side buttons; Back: host | Inspection `GolfHistoryActivity.cpp:147,175`; existing history tests |
| Archived round menu | Card, Hole by hole, Statistics, Send, Delete round/remove player: tap row; scroll/focus: vertical swipe/side buttons; delete opens confirmation; Back: host | Inspection `GolfHistoryRoundMenuActivity.cpp:56,144`; existing archive/player tests |
| Hole review | Previous/next hole: horizontal swipe or side buttons; Back: host | Inspection `GolfHoleReviewActivity.cpp:44`; shared edge-exclusion and review-format tests |
| Statistics | Read-only facts; Back: edge gesture or existing body tap; Home: host | Inspection `GolfStatisticsActivity.cpp:34`; existing statistics/layout tests |
| Trends | Read-only facts; Back: host; Home: host | Inspection `GolfTrendsActivity.cpp:153`; existing trends tests |
| Career stats | More/previous page: horizontal swipe or side buttons; Back: host; Home: host | Inspection `GolfCareerStatsActivity.cpp:166`; shared edge-exclusion and existing career-stat tests |
| Messages, including empty/error states | Dismiss: body tap or edge Back; Home: host | Inspection `GolfMessageActivity.cpp:17`; shared footer policy tests |
| Confirmation dialogs (Finish, Abandon, Delete/remove player) | Shared utility, unchanged, touch-native: tap Cancel/Confirm; outside tap or Back cancels | Inspection `ConfirmationActivity.cpp:17,64`; theme skips hints on touch |
| Player-name keyboard | Shared utility, unchanged, touch-native: tap/hold keyboard keys, delete, mode/shift, cursor position, OK; Back cancels | Inspection `KeyboardEntryActivity.cpp:506,930`; theme skips hints on touch |

## Resource bounds

No framebuffer or per-frame layout/drawing buffer allocation was added. Visible Recent cards and hub previews use the existing CourseStore JSON loader, including its temporary parser allocations; the course and catalog-info destinations are reused in activity scratch. Layout and drawing use fixed rectangles, integer figures and bounded strings. The picker retains its existing region array (≤64), ordinal/rating arrays (≤512), and sixteen row buffers; added figure/unit arrays and the 256-byte hub preview are fixed activity members. A catalog-info scratch object is stored in the existing one-time checked picker scratch allocation, rather than on the C3 task stack. Course info no longer stores sixteen fact-row buffers.

The additional temporary `GolfCourseFile[32]` allocation is checked with `makeUniqueNoThrow`, scoped only around the hub's owner count, and released before opening child screens; at roughly 1.7 KB it exceeds the prescribed 256-byte stack budget. Owner lists retain their pre-existing bounded 32-course summaries, with a map-availability flag; they do not load catalog names. Map availability uses HAL file existence checks and honours both explicit `mapsDir` and legacy name slugs, including a map present only on a later hole. It introduces no heap buffer. No SDK API, persistence format, framebuffer, or storage-write policy changed.

## Icons

Fetched only history, flag, map-pin and table SVGs from pinned `lucide-static@0.468.0` on unpkg, then generated 16/24/32 assets with the SDK icon generator. The original icons' release was not recorded in this repository; their original bitmap data and descriptors were preserved. A comparison against the parent confirmed all 54 original definitions (27 bitmap arrays and 27 descriptors) unchanged apart from whitespace. The manifest records the new version. Source icons remain generation-only dependencies.

## Verification

- macOS (`uname -s`: Darwin); required SDK commit `f831c1e447a21cfc7def620bb7c9c783e416a0a4` initialized from the existing local P3 SDK after GitHub clone failed over HTTP/2.
- Opened `docs/golf/design/catalog-blocks.html` in Orca's browser.
- Full host build and `ctest`: **565/565 passed**, with `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
- `GOLF_CATALOG_DIR=/tmp/golf-catalog-full ctest -R golf_catalog_check`: **passed**.
- Firmware `golf-x4pro`, `golf`, and `default`: **all passed after the final code edit**; output hashes are below.
- Repository formatting wrapper and `git diff --check` pass. Generated i18n headers, `.pio`, host build files, temporary SVGs/tooling and local build configuration are not staged.

Host logs are in `/tmp/golf-p4-host-final3-build.log`, `/tmp/golf-p4-ctest-final.log`, `/tmp/golf-p4-catalog-check-final.log`; firmware log is `/tmp/golf-p4-firmware-verified.log`.

## Hardware verification still required

Use the retained golf binaries on the appropriate boards in the normal human test workflow. Check all rows above by touch on X4 Pro, including left-edge swipes near paging areas, and compare X4 menus and all screens outside §43 with the previous firmware. Check portrait/inverted/landscape orientation behavior and e-ink refresh appearance. With debug logging, watch existing catalog-level heap messages and measure `ESP.getFreeHeap()` before/after repeated picker → info → maps/tees → setup → Back cycles; confirm free heap remains above 50 KB and returns to baseline. No device test, flash or measured performance/memory-saving claim is made here.

## Retained firmware outputs

`build/golf-p4/` is ignored; the original ELF/BIN outputs remain under `.pio/build/<environment>/`. The sizes below are linked static RAM and program flash, not runtime heap measurements.

| Environment | Static RAM | Program flash | Retained BIN | SHA-256 |
|---|---:|---:|---|---|
| default | 56,164 B | 5,445,927 B | `.pio/build/default/firmware.bin` | `22d68d2cd52a7a20f8ef206b672deff589767e6224c03f0ac7046d0f2f4037aa` |
| golf | 57,276 B | 5,664,529 B | `build/golf-p4/golf.bin` | `95546cd66c3fb020bc74de668f54b52d9678af770cebe6624860a7b8b0655d39` |
| golf-x4pro | 90,700 B | 5,552,394 B | `build/golf-p4/golf-x4pro.bin` | `7f2be23e52ec9c0036b58e3bf16723e382d6e8c7584d2b9d49b258431141fa56` |

The clean full build reported the existing WebSockets `NetworkClient::flush()` deprecation warning in the untouched dependency. No new-source warning or build error remains.
