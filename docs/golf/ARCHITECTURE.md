# Golf Scorecard — Architecture

Authoritative architecture for the golf scorecard app built on this CrossPoint fork.
Owned by the architect. **Workers implement against this document and
[CONTRACTS.md](CONTRACTS.md); they do not amend either one.** If a task cannot be
completed without violating something here, stop and report the conflict rather than
improvising around it.

## 1. Product scope

An offline, single-player golf scorecard that runs entirely on an Xteink X4.

* Scoring, storage, and review all happen on the device.
* Rounds are files on the SD card, retrieved via the existing CrossPoint webserver
  or by removing the card.
* No companion app, no cloud, no network dependency on the course.

Three values are captured per hole and only these three: **total strokes**, **putts**,
and **strokes played from inside 100 yards**.

Explicitly out of scope: multi-player, fairways hit, greens in regulation, penalties,
sand saves, per-hole notes, GPS, shot tracking.

## 2. Why this lives in a fork

CrossPoint's own `SCOPE.md` lists *"Interactive Apps: No notepads, calculators, or
games"* under Out-of-Scope, and its Phase 1 roadmap has temporarily closed new network
connectors. This app is never going upstream, and no work here should be shaped around
the hope that it might.

The consequence that matters is **rebase cost**. Upstream `develop` moves fast and
recently refactored the whole activity system. A fork that scatters edits across
upstream files becomes unmergeable within weeks.

### The three-touchpoint rule

All new code lives in **new directories**. Exactly three upstream files may be
modified, all behind the `CROSSPOINT_GOLF` build flag:

| File | Budget | Change |
| --- | --- | --- |
| `src/activities/home/HomeActivity.cpp` | ~21 lines | One menu row: item count, loop dispatch, and render, all `#if CROSSPOINT_GOLF` |
| `platformio.ini` | ~8 lines | `[env:golf]` extending `default`, adds `-DCROSSPOINT_GOLF=1` |
| `src/main.cpp` | ~14 lines | Guarded resume-into-round branch in the boot routing chain, plus a golf state flush in `enterDeepSleep()` beside the existing `APP_STATE.saveToFile()` |
| `test/CMakeLists.txt` | 1 marker + 1 line per suite | `# --- golf (fork) ---` marker + one `add_subdirectory()` per golf test suite |

*Amended 2026-08-29: `test/CMakeLists.txt` was added as a fourth touchpoint after a
worker correctly reported that M0 required registering test suites there while
acceptance allowed only one upstream file. The file uses explicit `add_subdirectory()`
calls with no globbing, so registration cannot avoid it. Two appended lines is the
low-conflict kind of edit this rule is meant to permit — the rule targets scattered
semantic edits, and running the golf suites under the same `unit-tests` gate as
everything else is worth more than the marginal rebase cost. Append golf suites at the
very end of the list, below a `# --- golf (fork) ---` comment, so a rebase conflict is
obvious and mechanical to resolve.*

*Touchpoint swapped 2026-08-29, BootActivity.cpp -> main.cpp. M1b-2 demonstrated that
`BootActivity` cannot express resume-on-wake, and I verified it: deep-sleep wake is
`BootResume::SplashlessWake`, which never calls `goToBoot()`, so `BootActivity` is not
entered on the one path that matters; and the routing chain at `main.cpp:517-543`
unconditionally replaces the current activity afterwards. No upstream idiom avoids the
touch, because the boot routing decision exists only in `main.cpp`. The golf branch sits
AFTER the recovery-firmware and panic/crash-report branches — a resumed round must never
pre-empt a crash report or recovery mode — and before the reader/home routing. This is a
SWAP, not an addition: the count stays at four.*

*Budget restated as a rule rather than a fixed count 2026-08-29, after M1a pointed
out that a fixed number goes stale every time a suite is added. Originally corrected to 3: the worker correctly pointed out that the
required marker comment plus two registrations is three lines, not the two the table
originally stated. The marker was my requirement, so the budget was mine to fix.*

**Touching any other upstream file requires architect sign-off before the work
starts.** The standing tie-breaker, set 2026-08-29: *when upstream already provides an
idiom that avoids touching a new file, use the idiom.* M0.1 surfaced the case that
established it — `scripts/git_branch.py` injects `CROSSPOINT_VERSION` only for the
`default` and `sticky` envs, so `env:golf` failed to compile against
`HalSystem.cpp`'s use of that macro. Adding `golf` to the script's tuple would have
been a one-word change but a fifth touched file. `[env:slim]` (platformio.ini:202) and
`[env:x4pro]` (:265) already solve it by naming their own version in build_flags —
`-DCROSSPOINT_VERSION=\"${crosspoint.version}-slim\"` — so `env:golf` does the same
with a `-golf` suffix and the touchpoint count stays at four. The accepted cost is that
a golf panic dump names the variant but not the exact commit, exactly as `slim` does. `[env:default]` must continue to build a stock reader with no golf code
compiled in.

*HomeActivity budget corrected from ~4 to ~21 lines, 2026-08-29. The original estimate
was naive: adding a menu row genuinely requires three separate edits — `getMenuItemCount`,
the `loop()` dispatch, and `render()`. M1b-3 also deliberately declined to add a
`HomeMenuItem` enum value, because that would touch `ActivityManager.h` as a fifth
upstream file; it dispatches on index position instead and documents why in a comment.
That is the §2 tie-breaker applied without being asked.*

Rebasing is `git rebase upstream/develop`, expected monthly.

## 3. Repository layout

```
main                     our fork; based on upstream/develop
upstream/develop         the baseline we rebase onto (remote: upstream)
```

New code:

```
src/golf/
  GolfRound.h              the round struct (see CONTRACTS.md §1)
  GolfRoundStore.{h,cpp}   PersistableStore<GolfRoundStore> -> /golf/state.json
  CourseStore.{h,cpp}      enumerate + load /golf/courses/*.json
  RoundArchive.{h,cpp}     write round file, append index.csv, read index
  GolfStats.{h,cpp}        pure functions over GolfRound -> derived figures

src/activities/golf/
  GolfHomeActivity.{h,cpp}        : UiListActivity
  GolfSetupActivity.{h,cpp}       : UiListActivity
  GolfScoringActivity.{h,cpp}     : Activity + UiAppHost    <- the screen that matters
  GolfCardActivity.{h,cpp}        : Activity + UiAppHost
  GolfHistoryActivity.{h,cpp}     : UiListActivity
  GolfRoundDetailActivity.{h,cpp} : Activity + UiAppHost
  GolfCourseEditActivity.{h,cpp}  : UiListActivity          (built last)

docs/golf/
  ARCHITECTURE.md    this file
  CONTRACTS.md       hard interfaces; workers must not deviate
  tasks/             per-milestone worker briefs
  reviews/           architect review notes, one per milestone
```

`GolfScoringActivity` derives from `Activity` and uses `UiAppHost` **directly**, not
`UiListActivity`. It is not a list, and forcing it into the list protocol makes it
fight the base class on every input event.

## 4. Rules inherited from CrossPoint

`AGENTS.md` at the repository root is authoritative for embedded coding standards and
is auto-loaded by agent tooling. Read it. The rules that bite hardest here:

* **~380 KB RAM is a hard ceiling.** ~47 KB of it is already the display framebuffer.
  Justify every heap allocation; prefer static and stack.
* **No `xTaskCreate` inside activities.** Nothing in this app needs a background task.
* **State that `render()` reads and `loop()` writes must be mutated under a
  `RenderLock`.** They run on different FreeRTOS tasks.
* **Never hold a `RenderLock` across a blocking call**, SD writes included. Take the
  lock, mutate, release, then flush.
* **Never invoke `clang-format` directly.** Use `./bin/clang-format-fix -g`.
* **Cite file paths and line numbers** as evidence when justifying a change.

## 5. E-ink refresh policy

The HAL exposes `FULL_REFRESH`, `HALF_REFRESH`, and `FAST_REFRESH` only. There is no
windowed partial-update API, so every repaint is whole-buffer.

* Counter or hole change -> deferred `requestUpdate()`, painted `FAST_REFRESH`.
  Deferred batching means several mutations in one loop pass produce one paint, which
  is what makes hold-to-repeat viable.
* Every 8th paint -> promote to `HALF_REFRESH` via `promoteNextRefresh()` to clear
  accumulated ghosting.
* Entering the scorecard table -> `HALF_REFRESH`. Dense grids show ghosting worst.
* Round summary on finish -> `FULL_REFRESH`.

## 5.1 Measured baseline (2026-08-29)

Upstream `develop` @ `b42927b`, env `default`, no golf code compiled in:

| Figure | Value |
| --- | --- |
| `firmware.bin` | 5,427,632 bytes (5.18 MB) |
| `app0` partition | `0x640000` = 6,553,600 bytes |
| Headroom | ~1.07 MB (~83% of the slot already used) |
| Clean build | ~4m50s · incremental ~1m30s |

*This corrects the design document's claim that flash budget is "a non-issue." It is
ample for a golf app measured in kilobytes, but the slot is already 83% full, so any
future work that pulls in a sizeable dependency must check the binary still fits.*

## 5.2 On-device confirmation (2026-08-29)

Hardware bring-up completed on the owner's X4. Official CrossPoint **v1.5.0** flashed
via the web installer; the unit was bought direct from xteink.com and is **not
USB-locked**, so no unlocker was involved and the documented revert path to official
Xteink firmware stands.

Two design assumptions were previously unverified and are now confirmed:

| Assumption | Status | Consequence |
| --- | --- | --- |
| `FAST_REFRESH` is quick enough for one-press-one-repaint | **Confirmed** — page turns read as near-instant | The §4 input model stands as designed. Live counters with a press per stroke are viable; hold-to-repeat stays a convenience rather than becoming the primary mechanism. |
| An SD card is present and mountable | **Confirmed** — card in, files browsable | The whole persistence design (§6, CONTRACTS §5) is sound as specified. No fallback to NVS or SPIFFS is needed. |

A page turn is the same `FAST_REFRESH` call every scoring-screen button press will make,
which is why it was the proxy worth asking about before building the screen.

Note the owner's stock install is v1.5.0 and `[crosspoint] version` is also `1.5.0`, so
fork builds identify as `1.5.0-golf` against exactly that baseline.

## 5.3 Plan revision: course loading moved M4 -> M1b (2026-08-29)

The published milestone plan put the course library at M4 and had M1 fall back to a
"quick round" assuming par 4 on every hole. That sequencing was wrong, and the owner's
question about course data is what exposed it.

A scoring screen with no course has nothing to score against. Under a par-4-everywhere
fallback the stroke counts are right but **to-par is wrong on every par-3 and par-5**,
which corrupts the running score, the to-par badge, and the whole scorecard. The first
real round would produce a card that cannot be trusted — which is the one failure mode
that makes a scorecard worthless.

**Decision:** M1b gains a minimal `CourseStore` — enumerate `/golf/courses/*.json`,
validate per CONTRACTS §5.1, and pick one when starting a round. It is a list screen
over a directory, and `src/activities/home/FileBrowserActivity` is a close existing
template. Quick round survives as the fallback for turning up somewhere unplanned.

What stays at M4 is the *authoring* side: the on-device course editor. That remains
last deliberately, because hand-authoring a small JSON on a computer and dropping it on
the SD card takes about a minute, while entering 18 pars and 18 yardages through
stepper rows on a 7-button device is genuinely unpleasant. See
`docs/golf/examples/` for the template and field guide.

## 5.4 Deliberate deviation: no i18n for golf screens

`AGENTS.md` requires user-facing strings to go through `tr()`. Golf screens do **not**,
and that is a considered exception rather than an oversight.

`tr(id)` resolves only a generated `StrId`, produced by `scripts/gen_i18n.py` from the
YAML files in `lib/I18n/translations/` — 24-plus languages. Adding golf strings would
mean editing generated tables and every translation source, turning a four-file
touchpoint surface into dozens and guaranteeing a rebase conflict in each one. The
`tr()` rule exists to keep *upstream* translatable; it does not usefully bind a
single-user fork feature that will never be translated.

Golf screens therefore use English literals, collected in **one header of string
constants** rather than scattered through the activities, so a future i18n pass has a
single place to work from. If this fork is ever shared beyond its author, i18n becomes a
real task and that header is where it starts.

## 6. Review gates

Every milestone is reviewed by the architect before the next one starts. A worker's
own report is not evidence; claims are verified against the code.

Upstream's own pre-PR checks are the mechanical floor, all three required to pass:

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

Plus, for this fork:

```bash
pio run -e golf          # the fork build
pio run -e default       # must still build a stock reader with no golf code
```

Beyond green builds, review checks: the three-touchpoint budget, the CONTRACTS.md
invariants, heap discipline, `RenderLock` correctness, and that acceptance criteria in
the task brief are actually met rather than asserted.
