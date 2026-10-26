# Task v8-P5: Recent count only, and catalog search

Worktree: `/Users/noah/orca/workspaces/scorecard/course-catalog-p5`, branch
`Lucidedream/course-catalog-p5`, based on `main` (`00dbb1f4`). First:
`git submodule update --init freeink-sdk`.

Spec: **CONTRACTS-V2 §45** (appended in this worktree, uncommitted), building on §41.4–41.5,
§43 and §44. The picker lives in `GolfCoursePickerActivity`; the return context is in
`GolfNavigation`/`GolfSetupReturn`.

Build:
1. §45.1: Recent's detail box shows the count only.
2. §45.2: the Search bar on Regions, `KeyboardEntryActivity`, a pure host-testable
   matcher (case-fold, all words, empty query), the Search results level, and the return
   context including the query.
3. Add Lucide `search` to `golfTileIcons.manifest`. Fetch only that SVG (e.g. from unpkg
   `lucide-static`), regenerate, and confirm the existing icons are byte-identical.
4. New `tr()` keys in `english.yaml`.

Verify:
- Host tests (synthetic only; the repo is public):
  - the matcher, covering case, multiple words, no match, empty and over-length queries;
  - grid rows-per-page selection;
  - the return context round trip with a query;
  - full `ctest`, run with
    `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
- `GOLF_CATALOG_DIR=$HOME/golf-catalog-export ctest -R golf_catalog_check`.
- Builds `golf-x4pro` and `golf` with the installed `~/.platformio` (keep both outputs),
  then `default`. Run `./bin/clang-format-fix -g` and `git diff --check`.

Commits (local; no push, no flash):
1. `docs: CONTRACTS-V2 §45 Recent count and catalog search`.
2. `feat: Recent shows a count; search the catalog from By region (§45)`.

No assistant attribution trailers. Ask rather than invent product behaviour.
