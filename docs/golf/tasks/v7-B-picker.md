# Task v7-B: the front/back nine picker (CONTRACTS-V2 §37.10, §37.11 navigation)

Branch: `feature/course-nines`. Worker A has already landed §37.1–§37.9/§37.11 in this
worktree (uncommitted): `GolfCourse` with nines, `GolfNineSelection`,
`golfDefaultNineSelection`, `golfNineSelectionValid`, and
`GolfPlayerSetupActivity(file, selection)`. Read `CLAUDE.md` and CONTRACTS-V2 §37
(especially §37.10 and §37.11) first, then read the code as it stands — build on it, do
not rework it.

## In scope

* `src/activities/golf/GolfNineSelectActivity.{h,cpp}` exactly per §37.10 (two phases,
  four rows, swap-on-collision, focus rules, Back behaviour, load-failure header).
  Model it on `GolfPlayerSetupActivity` (UiListActivity, `golfui` chrome, footer, RenderLock,
  `requestUpdate`, enabled/focus handling, front-button nav via the existing helpers).
* `openGolfNineSelect(manager, renderer, input, file, selection)` in `GolfNavigation`.
* `GolfSetupActivity::activateIndex`: load nothing big — open the nine picker with
  `golfDefaultNineSelection` when the row's course has `nineCount >= 2` (the summary
  carries `nineCount`), else player setup with `{0, GOLF_NO_NINE}`.
* `GolfPlayerSetupActivity` Back from the count/players phase: to the nine picker with the
  current selection when `nineCount >= 2`, else to the course list.
* i18n: add `STR_GOLF_CHOOSE_NINES`, `STR_GOLF_SWAP_NINES`, `STR_GOLF_CONTINUE` (only if
  no equivalent key exists — check `english.yaml`), `STR_GOLF_COURSE_LOAD_ERROR` if
  worker A did not already, and a nine subtitle format key (e.g.
  `STR_GOLF_NINE_DETAIL_FORMAT` "Par %u · %u yds" and a par-only variant) to
  `lib/I18n/translations/english.yaml` only. Regenerate with
  `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/` (generated files are
  gitignored).
* Put the pure picker logic (choose nine for slot with swap-on-collision, swap, nine par
  total, longest-tee yardage for a nine) in a small pure header/function set and host-test
  it in a new or existing `test/golf_*` suite.

## Constraints

CLAUDE.md applies: no stack `GolfCourse`, `makeUniqueNoThrow` for the activity, `tr()` for
all UI text, `snprintf` into fixed buffers, logical buttons only, orientation-agnostic
layout via `golfui` / `renderer.getScreenWidth()`. Comments short, merged-state voice.

## Done means

1. Host tests all pass (report count): `cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j`.
2. `pio run -e golf` succeeds, no new warnings (report flash %).
3. `./bin/clang-format-fix -g`.
4. Do **not** commit. Report files changed and any deviation from §37.10.

## Review fix from v7-A (you own it, since you are editing this file anyway)

`GolfPlayerSetupActivity::onBackButton` lost its Players-phase branch: Back on the
roster must return to the **Count** phase exactly as before (`git show HEAD:src/activities/golf/GolfPlayerSetupActivity.cpp`,
`onBackButton`). Only Back from the **Count** phase leaves the activity — to the nine
picker (same selection) when `nineCount >= 2`, else to the course list. `GolfSetupActivity`
must use `golfDefaultNineSelection` rather than an inline `{0, …}` literal.
