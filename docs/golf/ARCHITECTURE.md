# Golf Scorecard — Architecture

Authoritative architecture for the golf scorecard app built on this CrossPoint fork.
Owned by the architect. **Workers implement against this document and
[CONTRACTS.md](CONTRACTS.md); they amend either one only when an approved contract task
explicitly requires it.** Otherwise, stop and report a conflict rather than improvising
around it.

## 1. Product scope

An offline scorecard for one to four golfers that runs entirely on an Xteink X4.

* Scoring, storage, and review all happen on the device.
* Rounds are files on the SD card, retrieved via the existing CrossPoint webserver
  or by removing the card.
* No companion app, no cloud, no network dependency on the course.
* Four stable player slots are stored in every round. A slot participates only when its
  fixed tee selection is Blue or White; `NotPlay` disables it.

Each enabled player captures putts, strokes from inside 100 yards (including putts),
strokes from outside 100 yards, and packed penalty events. Gross total is derived.
Course par and stroke index are shared; yardages are per player because tee choices may
differ.

Explicitly out of scope: more than four players, player handicaps or net scoring,
fairways hit, greens in regulation, sand saves, per-hole notes, GPS, and shot tracking.

## 2. Why this lives in a fork

CrossPoint's own `SCOPE.md` lists *"Interactive Apps: No notepads, calculators, or
games"* under Out-of-Scope, and its Phase 1 roadmap has temporarily closed new network
connectors. This app is never going upstream, and no work here should be shaped around
the hope that it might.

The consequence that matters is **rebase cost**. Upstream `develop` moves fast and
recently refactored the whole activity system. A fork that scatters edits across
upstream files becomes unmergeable within weeks.

### The bounded-touchpoint rule

Golf implementation stays in its dedicated directories. Only the upstream integration
surfaces listed here may be modified, with executable changes behind the
`CROSSPOINT_GOLF` build flag where applicable:

| File | Budget | Change |
| --- | --- | --- |
| `src/activities/home/HomeActivity.cpp` | ~21 lines | One menu row: item count, loop dispatch, and render, all `#if CROSSPOINT_GOLF` |
| `platformio.ini` | ~8 lines | `[env:golf]` extending `default`, adds `-DCROSSPOINT_GOLF=1` |
| `src/main.cpp` | ~14 lines | Guarded resume-into-round branch in the boot routing chain, plus a golf state flush in `enterDeepSleep()` beside the existing `APP_STATE.saveToFile()` |
| `test/CMakeLists.txt` | 1 marker + 1 line per suite | `# --- golf (fork) ---` marker + one `add_subdirectory()` per golf test suite |
| `lib/I18n/translations/*.yaml` | Scorecard keys only | Source translations for user-facing Scorecard labels; generated headers remain untouched |

*Multiplayer/i18n amendment:* the translation-source row supersedes the historical
"exactly three/four" counts below. It permits only Scorecard string keys and does not
open unrelated upstream files to modification.

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
  GolfRound.h              fixed four-slot round model (see CONTRACTS.md §1)
  GolfRoundStore.{h,cpp}   PersistableStore<GolfRoundStore> -> /golf/state.json
  CourseStore.{h,cpp}      enumerate + load /golf/courses/*.json
  RoundArchive.{h,cpp}     write round file, append index.csv, read index
  GolfStats.{h,cpp}        pure functions over shared round + explicit player score

src/activities/golf/
  GolfHomeActivity.{h,cpp}         : UiListActivity
  GolfSetupActivity.{h,cpp}        : UiListActivity
  GolfPlayerSetupActivity.{h,cpp}  : UiListActivity
  GolfScoringActivity.{h,cpp}      : Activity + UiAppHost    <- the screen that matters
  GolfCardActivity.{h,cpp}         : Activity + UiAppHost
  GolfPlayerSelectActivity.{h,cpp} : UiListActivity
  GolfHistoryActivity.{h,cpp}      : UiListActivity
  GolfTrendsActivity.{h,cpp}       : Activity + UiAppHost
  GolfRoundDetailActivity.{h,cpp}  : Activity + UiAppHost
  GolfCourseEditActivity.{h,cpp}   : UiListActivity          (built last)

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
  Justify every heap allocation; prefer fixed members and small stack values.
* **No `xTaskCreate` inside activities.** Nothing in this app needs a background task.
* **State that `render()` reads and `loop()` writes must be mutated under a
  `RenderLock`.** They run on different FreeRTOS tasks.
* **Never hold a `RenderLock` across a blocking call**, SD writes included. Take the
  lock, mutate, release, then flush.
* **Never invoke `clang-format` directly.** Use `./bin/clang-format-fix -g`.
* **Cite file paths and line numbers** as evidence when justifying a change.

### 4.1 Multiplayer memory boundary

Multiplayer is a fixed-capacity extension, not a dynamic roster. `GolfRound` embeds four
`GolfPlayer` records and each embeds exactly 18 holes of yardage, score counters, and
packed penalties. `src/golf/GolfRound.h` locks the layout with size assertions:
144 bytes per `GolfPlayerScore`, 206 bytes per `GolfPlayer`, and 906 bytes for the full
round. Enabling another slot performs no `new`, `malloc`, or vector growth; the 906-byte
worst case is reserved once, so runtime heap fragmentation does not vary with player
count. The increase from the prior 254-byte round is a fixed 652 bytes.

That predictable size is still too large for an automatic local on a small embedded task
stack. The live round belongs in the persistent singleton or as a member of an already
heap-allocated activity. Domain helpers take `GolfRound&`, `GolfPlayer&`, or
`GolfPlayerScore&`; rendering and mutation paths do not copy a whole round. Persistence
uses one checked `makeUniqueNoThrow` staging allocation when it needs transactional
decode/archive semantics, logs OOM, and never allocates inside a player or hole loop.

History and Trends use a separate fixed-capacity player-selector activity. It retains one
`GolfPlayerNamesReader`, one index-recovery migrator, and one 128-byte streaming chunk.
Each index scan or post-child rescan needs one checked `HalFile::Impl` handle allocation;
it logs OOM and publishes unavailable rows, while row parsing itself performs no
allocation. The selected data screen receives one immutable stable-slot number plus a
copied fallback name, so neither data activity retains all four names or a tab bar.
History and Trends keep checked inactive reader storage and publish a complete
slot-specific snapshot under `RenderLock`; target payload accounting is recorded in
`CONTRACTS-V2.md` §16.6.

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

## 5.4 Course-to-roster device feedback

Selecting a course immediately produces a truthful playable draft when the source permits
it. Setup enables P1 Noah on Blue when `CourseStore::resolveTee()` can resolve Blue, or on
exact White when Blue is unavailable. A course with both an empty tee label and no yardage
supports Blue and White as zero-yard selection-only choices and defaults P1 to Blue. P2–P4
stay `NotPlay`. Noncanonical labels and unlabeled nonzero yardage are not guessed, so those
courses retain the all-disabled draft and require corrected course data. The neutral round
initializer and legacy decoder defaults remain unchanged.

Because activity is still defined solely by `tee != NotPlay`, Complete is enabled on the
first render whenever that initial P1 selection is truthful. This is device feedback, not
a second stored enabled flag and not an implicit yardage assignment.

## 5.5 Internationalization

The earlier single-user exception for hard-coded English golf screens is retired.
Scorecard is now a multi-user feature and follows the repository rule: every user-facing
label goes through `tr()`, including setup labels for Player, Not play, Blue, and
White. Serialized `TeeSelection` values remain canonical language-independent tokens;
translated display text is never written to a round file. Player names are user data and
therefore are not passed through `tr()`.

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
