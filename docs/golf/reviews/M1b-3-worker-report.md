# M1b-3 — Worker report (build + wiring)

Worker: claude, dispatch `ctx_d75992712f6c`. Not a milestone review — this is the
build/wiring worker's own report, for the architect to verify independently per the
"a worker's own report is not evidence" rule in ARCHITECTURE.md §6.

## What was done

Four compile fixes, in the golf-owned files only:

1. **`GolfStrings::CHANGE` renamed to `SWITCH_FIELD`** (`GolfStrings.h`). Confirmed via
   `Arduino.h:62` that `CHANGE` is `#define CHANGE 0x03`. Audited every other identifier
   in `GolfStrings.h` against the full candidate list in the task brief (RISING,
   FALLING, ONLOW, ONHIGH, HIGH, LOW, INPUT, OUTPUT, DEFAULT, SERIAL, PI, LSBFIRST,
   MSBFIRST, B0..B11111111) by grepping the actual Arduino-ESP32 core headers — `CHANGE`
   was the only collision; ESP32's core defines no `Bxxxxxxxx` binary-literal macros
   (that's AVR-only Arduino). One call site updated (`GolfSetupActivity.cpp:244`).
2. **`GUI.getMetrics()` → `UITheme::getInstance().getMetrics()`** in the four cited
   files (`GolfMessageActivity.cpp`, `GolfHomeActivity.cpp`, `GolfRoundMenuActivity.cpp`,
   `GolfSetupActivity.cpp`). Confirmed against `UITheme.h`: metrics live on the
   `UITheme` singleton, `BaseTheme` has no such member.
3. **The reported `Rect` ambiguity did not reproduce.** After fix #1 alone, the
   full build succeeded with no changes to `GolfMessageActivity.cpp`'s `Rect{...}`
   calls. My reading: `GolfStrings.h`'s `CHANGE` corruption (`inline constexpr char
   0x03[] = "Change";`) is a parse error inside a header included by every golf `.cpp`
   file, and GCC's recovery from that kind of error commonly produces spurious
   downstream diagnostics for the rest of the same translation unit — which is what the
   "ambiguous Rect" report most likely was. `Rect{0, y, w, h}` as a function argument is
   direct-list-initialization, which does consider `BaseTheme`'s `explicit
   Rect(int,int,int,int)` constructor (explicit constructors are excluded only from
   *copy*-list-initialization, not direct), so the call was never actually ill-formed.
   **No second fix was needed or made.** Flagging this as a corrected diagnosis, not a
   worked-around defect.
4. **`#include "util/ConfirmationActivity.h"` → `#include
   "activities/util/ConfirmationActivity.h"`** in `GolfRoundMenuActivity.cpp`. The class
   was not invented and does exist — `src/activities/util/ConfirmationActivity.h` — and
   is already the upstream idiom for a heading+body Cancel/Confirm dialog (six other
   call sites: `FontDownloadActivity`, `SdFirmwareUpdateActivity`, `RecentBooksActivity`,
   `FileBrowserActivity`, plus the two here). It wraps `OptionPopup` internally, so using
   it *is* "use `OptionPopup`" — just via the existing wrapper rather than rebuilding the
   popup wiring by hand, per the standing tie-breaker (ARCHITECTURE §2: use an existing
   idiom over touching/inventing a new one). The prior worker's call site
   (`makeUniqueNoThrow<ConfirmationActivity>(...)`, the
   `startActivityForResult(..., [this](const ActivityResult&){ completeAction(!result
   .isCancelled); })` callback) was already correct and needed no change — only the
   include path was wrong. Both Finish-round and Abandon-round still confirm before
   acting; the confirmation was never dropped.

## Upstream wiring (previously missing)

**(a) `src/activities/home/HomeActivity.cpp`** — one new menu row. `HomeMenuItem` (the
enum driving the existing switch-based dispatch) lives in `ActivityManager.h`, a file
outside the four-file budget, so adding a `GOLF` enum case was not an option. Golf is
instead appended as an extra row *after* Settings, entirely inside the already-budgeted
`.cpp`: `getMenuItemCount()` counts it, `render()` pushes its label/icon onto the
existing dynamic `menuItems`/`menuIcons` vectors (`Bookmark` icon — no existing icon
reads as "golf"; picking a different one from `BaseTheme.h`'s `UIIcon` enum is a
one-line change with no other touch), and `loop()`'s `activateSelection` lambda checks
for the golf row's index *before* the `HomeMenuItem` switch and calls `openGolfHome()`.
21 lines against the architecture doc's ~4-line estimate — the menu here is genuinely
dynamic (vectors, not a static row array), so reaching it needs touches in three
different functions; still exactly one upstream file touched.

**(b) `src/main.cpp`**, two edits:
- **Resume-on-boot**: an `else if (resumeGolfRound(activityManager, renderer,
  mappedInputManager))` branch inserted after `rebootedFromPanic` and before the
  `BootResume::Silent` branch — exactly the placement CONTRACTS §7 / ARCHITECTURE §2
  require. `resumeGolfRound()` (already written, in `GolfNavigation.cpp`) already
  implements every case in the brief: no `state.json` → returns false, unloadable file →
  shows an error activity, `archivedAs` set → clears the file without resuming or
  re-archiving (§6.1), valid open round → opens `GolfScoringActivity` at `currentHole`.
  No new logic was needed here, only the call site.
- **Sleep flush**: `flushGolfRoundForSleep()` added directly after `APP_STATE
  .saveToFile()` and before `activityManager.goToSleep(...)` in `enterDeepSleep()`,
  covering both the auto-sleep and power-button-hold paths per CONTRACTS §6. The
  function is a cheap no-op when no round is dirty (checked before touching SD).

## Gate results

| Gate | Result |
| --- | --- |
| `pio run -e golf` | **SUCCESS** — `.pio/build/golf/firmware.bin` = 5,456,432 bytes |
| Delta from 5,427,648 baseline | **+28,784 bytes** (0.44% of the 6,553,600-byte app0 partition) |
| `pio run -e default` | **SUCCESS** — firmware.bin = 5,427,632 bytes, byte-identical to the documented baseline |
| Golf symbols in `default` ELF | **0** (`riscv32-esp-elf-nm ... | grep -ci golf`) |
| Golf symbols in `golf` ELF | **164** — confirms the wiring actually references the code (was 0 before wiring, still compiling-but-dead-stripped) |
| Golf host tests | **48/48 passed** (`ctest`, llvm-based host toolchain per the task's macOS CLT workaround) |
| `./bin/clang-format-fix -g` | Reformatted the new/modified files on first run (they'd never been through the formatter); **second run is a byte-for-byte no-op** (verified via `shasum` before/after) — tree is now stable under the formatter |
| Upstream touchpoints | Exactly 4 files touched: `platformio.ini` (unchanged from prior milestone), `test/CMakeLists.txt` (unchanged from prior milestone), `src/activities/home/HomeActivity.cpp` (+21 lines, all `#if CROSSPOINT_GOLF`), `src/main.cpp` (+16 lines, all `#if CROSSPOINT_GOLF`) |

**RAM** (from the `golf` build's linker report, static/link-time figures — not a live
`ESP.getFreeHeap()` reading, which needs actual hardware): DRAM 123,317 / 321,296 bytes
used (38.38%), 197,979 bytes remaining. `.bss` 36,384 B, `.data` 20,017 B, `.text` (code
placed in DRAM) 66,916 B. This is only 256 bytes of DRAM more than the `default` build
(123,061 → 123,317), consistent with a handful of new static string tables and no
meaningful new global state. Live heap
headroom on-device is unverified and is squarely the "human tester" scope item in
CLAUDE.md's testing checklist (`ESP.getFreeHeap() > 50KB`).

**Note on the note above**: `clang-format-fix -g` only sees files `git ls-files
--modified` reports, which excludes untracked new files. To actually exercise the
formatter against the new golf files (not just silently skip them), I temporarily ran
`git add -N` on the new golf paths so they'd show as tracked-but-modified, ran the
formatter, verified idempotency, then `git reset` to return the index to its prior
(fully untracked) state. No commit was made and no file was staged when the work ended.

## Not done / out of scope for this dispatch

* `pio check -e default` (cppcheck) was not run — it isn't in the task's ACCEPTANCE list
  and AGENTS.md lists it as part of the architect's own review gate, not a build-fix
  deliverable. Flagging in case that's expected before sign-off.
* Live on-device verification (heap headroom, all 4 orientations, actual resume-from-
  sleep behavior, first real finish-round exercising the untested `RoundArchive`
  collision loop) is unchanged from prior milestones' "human tester scope" — still
  pending real hardware.
