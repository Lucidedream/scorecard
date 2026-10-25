# Task v8-P3: play catalog courses (picker in New round, Play row)

Worktree: `/Users/noah/orca/workspaces/scorecard/course-catalog-p3` (Mac disk), branch
`Lucidedream/course-catalog-p3`, based on `Lucidedream/course-catalog-p2` (`05027451`).
First: `git submodule update --init freeink-sdk`.

Spec: **CONTRACTS-V2 §41.5, the "`Play` mode and the Play row (P3)" block** (added in
this worktree, uncommitted). It builds on P2's `GolfCoursePickerActivity`,
`GolfCourseInfoActivity` and the §41.4 data layer; read `docs/golf/tasks/v8-P2-*` and
P2's report for how they work.

The owner can browse the 425 catalog courses in Course Map but cannot start a round on
one. That is the bug this task fixes.

## Build

1. `Mode::Play` in `GolfCoursePickerActivity`:
   - The New round tile opens it.
   - In Play mode, picking a course runs today's start flow (see §41.5).
   - My courses in Play mode keeps exactly today's New round list behaviour. Reuse
     `GolfSetupActivity`'s logic rather than duplicating it.
2. **Return context:** `GolfNavigation` keeps a small value (mode, level, region,
   sort, selected row) so Back from the nine picker or player setup returns to the exact
   picker row. Never write it to SD.
3. **Play this course** row on Course info: shown only when
   `golfDecideHomeEntry(...).showNew` is true. Back from the flow returns to Course info.
4. Rounds started from a catalog course must resume after a reboot, appear in History
   and Recent, and open the in-round map glance with that course's `bg-<id>` maps
   (`mapsDir`). Verify each through the existing paths; fix any that assume
   `/golf/courses`.
5. Strings via `tr()`: `STR_GOLF_PLAY_THIS_COURSE` plus anything else needed.

## Constraints

As in P2: CLAUDE.md, C3 RAM (one page of names, no unbounded buffers, locals under
256 B), `HalStorage` only, `GolfUiListActivity` lists for buttons and touch, short
comments. Do not change P2's Browse behaviour or the catalog formats.

## Verification

- Host tests (synthetic only; the repo is public):
  - the return-context round trip;
  - Play-row visibility for every `golfDecideHomeEntry` outcome;
  - `findByName` resolving a catalog-course round, and anything else you make pure.
  - Full `ctest`, run with
    `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
- `GOLF_CATALOG_DIR=/tmp/golf-catalog-full ctest -R golf_catalog_check`, the real
  425-course export (read it, never copy it).
- Builds `golf-x4pro`, `golf`, `default`, using the installed `~/.platformio` (do **not**
  set `PLATFORMIO_CORE_DIR`). If a library download stalls, copy the matching folders
  from `~/src/scorecard/.pio/libdeps/golf`. Keep `golf-x4pro` and `golf` outputs; delete
  `default` after it passes.
- `./bin/clang-format-fix -g` and `git diff --check`.

## Commits (local; no push, no flash)

1. `docs: CONTRACTS-V2 §41.5 Play mode` (contract + this brief).
2. `feat: play catalog courses from New round and Course info (§41.5)`.

Commit messages must not carry assistant attribution trailers. Report worker_done with
test counts, build sizes and how each item in Build was verified. Ask rather than invent
product behaviour.
