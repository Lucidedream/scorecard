# Task v8-P2: catalog courses on the device, and the Course Map picker (Browse mode)

Worktree: `/Volumes/My Passport/scorecard-worktrees/course-catalog-p2` (on the external
drive, **not** the Mac disk), branch `Lucidedream/course-catalog-p2`, based on
`Lucidedream/course-catalog-p1` (`dfc5bbc0`). First:
`git submodule update --init freeink-sdk`.

Spec: **CONTRACTS-V2 §41.4 (amended in this worktree, uncommitted) and §41.5**,
**Browse mode only**. The data formats you read are §41.3, already produced by P1's
exporter. Read the whole of §41, and §26/§30/§34/§40 for the screens you extend.

## In scope

1. **Data layer** (§41.4):
   - `GolfCourse::mapsDir` and the parser's `maps` field.
   - Every `/golf/maps/...` lookup uses `mapsDir`, else `golfSlug(name)`: the in-round
     glance (§26), the map browser (§30) and anything else that builds that path.
   - `GolfCourseFile` gains a source (built-in / owner / catalog).
   - `CourseStore::load` reads `/golf/catalog/courses/<filename>` for catalog files.
   - `CourseStore::findByName` searches built-ins, then owner files, then the catalog:
     a sequential `index.dat` scan comparing the 40-byte name field after trimming
     padding, then loading `bg-<id>.json`.
   - `GolfCourseCatalogInfo` and `golfParseCatalogInfo`.
   - Pure, host-testable readers for `regions.txt` and `index.dat` records with fixed
     buffers. Malformed lines and records are skipped and logged; a wrong record length
     or a bad header rejects the catalog, so By region is hidden.
2. **Picker**, `GolfCoursePickerActivity` with `Mode::Browse`, opened by the main menu's
   Course Map tile in place of today's `GolfCourseMapListActivity`:
   - Hub: Recent / My courses / By region, with the §41.5 visibility rules.
   - Recent: from `/golf/rounds/index.csv`, streamed with the existing CSV reader, newest
     first, 5 distinct names, resolved by `findByName`.
   - My courses: today's list behaviour. Reuse `GolfCourseMapListActivity`'s logic or
     fold it in; do not duplicate it.
   - Regions → Region courses, with the sort-toggle row, the row format and paging.
   - Picking a course opens **Course info**.
   - Back goes up one level; Back from the hub goes Home.
3. **Course info**, `GolfCourseInfoActivity`, as in §41.5:
   - Rows **Hole maps** (the existing browser) and **Tee info** (the existing page).
   - **No Play row yet**; that is P3.
   - An owner course with no catalog info shows only the facts it has.
4. **Strings:** every new user-facing string uses `tr()` with English YAML keys:
   `STR_GOLF_RECENT`, `STR_GOLF_MY_COURSES`, `STR_GOLF_BY_REGION`, `STR_GOLF_SORT_RATING`,
   `STR_GOLF_SORT_NAME`, `STR_GOLF_CATALOG_ROW_FORMAT`, `STR_GOLF_NO_RATING`,
   `STR_GOLF_NO_MAPS`, `STR_GOLF_DATA_BAIGOLF`, `STR_GOLF_RATING_LINE_FORMAT`, the six
   category labels, `STR_GOLF_HOLE_MAPS`, `STR_GOLF_TEE_INFO`, plus anything else you
   need. Reuse existing keys where they fit.

## Out of scope (P3)

New round's use of the picker, `Mode::Play`, the Course info Play row, and duplicate
linking. Do not change `GolfSetupActivity`'s flow.

## Constraints (CLAUDE.md)

- **C3 RAM:**
  - The picker never holds more than one page of names.
  - The region record list is `uint16_t[≤512]` and regions are `≤64`.
  - No `std::string`/`std::vector` growth in these paths.
  - Locals stay under 256 B; larger buffers are members or `makeUniqueNoThrow`.
  - Log `ESP.getFreeHeap()` with LOG_DBG on entering each picker level.
- **SD access** goes only through `HalStorage`/`HalFile`, with no explicit `close()` on
  locals.
- **Input:** all lists derive from `GolfUiListActivity`, so they work with buttons (X4)
  and touch (X4 Pro).
- **Comments:** short, written for the merged state.

## Verification

- **Host tests** (new, in `test/`):
  - the `regions.txt` and `index.dat` readers: good, malformed and bad-header input;
  - the rating sort, including unrated last and name ties;
  - Recent extraction from an index.csv fixture: distinct, newest first, capped at 5;
  - the `maps` field: valid, invalid and absent (falls back to the slug);
  - `golfParseCatalogInfo`: full, partial and malformed;
  - the `findByName` order, using whatever seam the existing CourseStore tests use.
  
  Use synthetic fixtures only; the repo is public (§41.1). Run all of `ctest` with
  `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
  Also run `GOLF_CATALOG_DIR=/Users/noah/.claude/jobs/ba718989/tmp/catalog-export ctest -R golf_catalog_check`
  (a real 5-course export outside the repo; read it, never copy it in).
- **Firmware builds** `golf-x4pro`, then `golf`, then `default`, one at a time.
  - The Mac's disk is almost full (another process is filling it). Keep everything in
    this worktree on the external drive.
  - Delete `.pio/build/<env>` after each env passes, except **keep `golf-x4pro` and
    `golf`** for flashing.
  - If a build fails for lack of space, stop and report rather than deleting anything
    outside this worktree.
- `./bin/clang-format-fix -g` and `git diff --check`.

## Commits (local only; do not push, flash or touch other worktrees)

1. `docs: CONTRACTS-V2 §41.4 keeps catalog facts out of GolfCourse` (the amended
   contract and this brief).
2. `feat: catalog courses and the Course Map picker (§41.4, §41.5)`.

Commit messages must not carry assistant attribution trailers. Report worker_done with:
host test counts; build sizes (RAM/flash %) for all three envs; the heap logging you
added; how Recent, My courses and By region behave when the catalog is absent; and
anything in §41.4 or §41.5 that was ambiguous. Ask rather than invent product behaviour.
