# Task v7-A: course model v2, nine composition, round `nines` (CONTRACTS-V2 §37)

Branch: `feature/course-nines` (already checked out in this worktree). Read `CLAUDE.md`
and **all of CONTRACTS-V2 §37** (`docs/golf/CONTRACTS-V2.md`, the last section) first —
it is the spec. This brief only scopes it.

## In scope

§37.1–§37.9, §37.11 and §37.13:

* New `GolfCourse` model (§37.2), removal of the listed APIs, pure parser
  `src/golf/GolfCourseParse.{h,cpp}` + rewritten `GolfCourseValidate` (§37.3).
* `CourseBuiltIns.h` in the new shape with sum `static_assert`s (§37.4).
* `CourseStore` enumerate/load/findByName, `GolfCourseSummary`, `golfSortCourses` (§37.5);
  update `GolfSetupActivity`, `GolfCourseMapListActivity`, `GolfHomeActivity` course count.
* Composition API (§37.6), `GolfRound::nines` + round file / `state.json` `"nines"` field +
  decoder rules + `golfCourseHoleNumber` (§37.7).
* Course-map hole numbering in scoring and the browser (§37.8), tee-info tabs per nine
  (§37.9), `pickGlanceTees` / `golfFormatTeeList` on `GolfCourse`.
* `GolfPlayerSetupActivity` takes `(GolfCourseFile, GolfNineSelection)` (§37.11).
  **For now** `GolfSetupActivity` opens player setup directly with
  `golfDefaultNineSelection(course)`, and player setup's Back goes to the course list.
  A second worker adds the nine picker afterwards (§37.10) — do NOT build
  `GolfNineSelectActivity`, but do add `GolfNineSelection` to the navigation signature
  so the picker can slot in.
* Host tests per §37.13 (rewrite `test/golf_course_validate`, `test/golf_course_order`,
  `test/golf_glance_tees`, round JSON tests, etc.). The example-file parse test should glob
  `docs/golf/examples/*.json` at test time; another worker is converting those files to v2
  in parallel, so if they are still v1 when you run, make the test skip v1 files with a
  clear message rather than failing, and tell me in your report.

## Out of scope — do not touch

`scripts/`, `docs/golf/examples/` (another worker owns them right now), the nine-picker
activity, any non-golf code beyond what the build forces.

## Constraints (from CLAUDE.md — enforced in review)

* `GolfCourse` (~700 B) is never a stack local; one heap scratch per scan via
  `makeUniqueNoThrow`, null-checked with `LOG_ERR`. No bare `new`, no `std::string` in
  hot paths, `constexpr` tables, `tr()` for any user-facing text, `.reserve()` before
  `push_back` loops in tests is fine to ignore.
* Comments short, written for the merged state (no "was/now" narration).
* Keep every non-golf build unaffected (`#if defined(CROSSPOINT_GOLF)` as the existing files do).

## Done means

1. `cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j`
   — all golf suites pass (report the count).
2. `pio run -e golf` succeeds with no new warnings (report flash % from the output).
3. `./bin/clang-format-fix -g` run.
4. Do **not** commit. Report: files changed, test count, flash %, any deviation from §37
   and why.
