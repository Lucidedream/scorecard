# Task: quick fairway hit (§46)

Worktree: `/Users/noah/orca/workspaces/scorecard/fairway-quick`, branch
`Lucidedream/fairway-quick`, based on `Lucidedream/course-catalog-p5` (`46afb97d`, P5,
not yet on main). First: `git submodule update --init freeink-sdk`.

Spec: **CONTRACTS-V2 §46** (appended in this worktree, uncommitted). Read §31 (fairway
storage, Mark sheet, file format, statistics), §12–13 (scoring fields and seeding), §33
(greenside bunker, a sibling per-hole bit), §38–39 (touch scoring and hole-band
swipes) and §44 (the Pro keeps the Scoring footer).

Build in this order: §46.1 data, statistics and JSON (with the legacy rule); §46.2 the chip
and the Mark sheet's Fairway row and marker icons; §46.3 the leave-hole prompt. Add the
icons `circle` and `circle-x` (fetch only those SVGs; existing icons byte-identical).

Verify:
- the §46.4 host tests (synthetic only; the repo is public), plus full `ctest` with
  `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`;
- a check that an existing v6 round fixture **without** `fairwaysAnswered` yields the same
  FIR numerator and denominator as before;
- builds `golf-x4pro`, `golf` and `default` with the installed `~/.platformio`. Copy
  the golf binaries to `/tmp/golf-fairway-artifacts/<env>/` (switching targets wipes
  `.pio/build`);
- `./bin/clang-format-fix -g` and `git diff --check`.

Commits (local; no push, no flash):
1. `docs: CONTRACTS-V2 §46 quick fairway hit`.
2. `feat: fairway answered state and FIR denominator (§46.1)`.
3. `feat: FW chip and leave-hole fairway prompt (§46.2, §46.3)`.

No assistant attribution trailers. Ask rather than invent product behaviour, especially on
multi-player prompt layout or anything that would change Scoring's existing gestures.
