# Task v7-A2: readability pass on the v7-A core (review findings)

Your v7-A work verified (514 tests, golf build 85.8%). Review asks for a readability pass
— **no behaviour change** — so the new code reads like the surrounding golf code
(CLAUDE.md: "match its comment density, naming, and idiom"):

* `src/golf/GolfCourseParse.cpp`: drop the `using E = …; using F = …;` aliases; descriptive
  names instead of `r`, `v`, `h`, `n`, `t`, `p`; braces on every `if`/`for` body; a blank line
  between functions and after the include block; split the nested ternary in `arrayValid`
  into a small helper that maps field → range error. Use `memcpy` with the validated
  length (as `CourseStore.cpp`'s `copyString` did) instead of `strcpy`. One short comment
  on why validation completes before `course` is written.
* `src/golf/CourseStore.cpp` `enumerate`: braces everywhere, `40` → `sizeof(GolfCourse::courseName)`
  or a named constant, a two-line comment stating the ordering/duplicate rules (filename
  order, lexicographically-first file wins a duplicate name, built-ins fill free slots
  after SD files). Keep the logic identical.
* `src/golf/GolfCourse.h`, `src/golf/CourseOrder.cpp` (`golfSortCourses`, compose
  functions): blank lines between declarations/functions, braces, one-line field comments
  for the non-obvious members (`GolfCourseTee::yards` indexing, `GolfNineSelection::back`
  sentinel, `GolfRound::nines`).
* `golfCourseHoleNumber`: guard `nines[i] == GOLF_NO_NINE` (return `roundHole + 1`).

Do NOT touch `src/activities/golf/*` — another worker is editing activities in parallel.
Done: host tests pass (same count or more), `pio run -e golf` succeeds
(`~/.platformio/penv/bin/pio`), `./bin/clang-format-fix -g`. Do not commit.
