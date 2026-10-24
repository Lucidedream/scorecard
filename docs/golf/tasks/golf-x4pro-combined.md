# Task: combined device-test firmware — course nines (§40) + X4 Pro frontlight sleep fix

Worktree: `/Users/noah/orca/workspaces/scorecard/golf-x4pro-combined`, branch
`Lucidedream/golf-x4pro-combined`, at `acddd940` (= `Lucidedream/course-nines-x4pro`:
origin/main + the two §40 course-nines commits). First: `git submodule update --init freeink-sdk`.

## Do

1. Apply the battery fix as **uncommitted** working-tree changes:
   `git apply --check docs/golf/tasks/x4pro-sleep-frontlight.patch && git apply docs/golf/tasks/x4pro-sleep-frontlight.patch`.
   It touches only `lib/hal/HalFrontlight.cpp`, `lib/hal/HalFrontlight.h`, `lib/hal/HalPowerManager.cpp`;
   course nines touches no `lib/hal` file, so it must apply cleanly. If it does not, stop and report —
   do not hand-merge. Do not change any other source.
2. Verify, sequentially:
   - `~/.platformio/penv/bin/pio run -e golf-x4pro` — **keep** this build output (it will be flashed).
   - `~/.platformio/penv/bin/pio run -e golf` — then delete `.pio/build/golf`.
   - `./bin/clang-format-fix -g` and `git diff --check` pass.
   Host tests are not needed: both halves already passed them separately and HAL is not in the host build.
3. `git status --short` must show exactly the 3 HAL files modified, plus the two untracked files
   in `docs/golf/tasks/`.

Do **not** commit, flash, push, or touch other worktrees/branches. Report worker_done with the
patch-apply result, both build results (flash/RAM %), and the firmware path
`.pio/build/golf-x4pro/firmware.bin`.
