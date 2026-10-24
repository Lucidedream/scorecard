# Task v8-P1: baigolf scraper and exporter (host only)

Worktree: `/Users/noah/orca/workspaces/scorecard/course-catalog-p1`, branch
`Lucidedream/course-catalog-p1`, based on `main` (`67b9b3ba`). First:
`git submodule update --init --reference /Users/noah/src/scorecard/freeink-sdk freeink-sdk`
(GitHub can be slow; that local checkout has the pinned commit).

Spec: **CONTRACTS-V2 §41.1–41.3 and §41.6**. It is in this worktree's
`docs/golf/CONTRACTS-V2.md`, uncommitted. Background:
`docs/golf/design/v8-course-catalog.md`. §41.4–41.5 are firmware work for a later task:
read them so the output serves them, but do **not** change firmware in this task.

## Build

1. `scripts/baigolf_scrape.py` and `scripts/baigolf_export.py` per §41.2–41.3. Standard
   library only, except Pillow for images. Pillow is not in the system `python3`; run the
   image code with `~/.platformio/penv/bin/python` (it has Pillow 12). The export script
   must fail with a clear message naming that interpreter if `PIL` is missing. Share
   parsing and validation in a small module (e.g. `scripts/baigolf_common.py`) rather
   than duplicating it.
2. `scripts/test_baigolf_export.py` and **synthetic** fixtures under
   `scripts/fixtures/baigolf/` covering every §41.6 bullet. Invent course names, ids and
   numbers. Never paste a real baigolf response or image (§41.1; the repo is public).
   Tiny generated PNGs are fine.
3. Host CTest `golf_catalog_check` in `test/`, following an existing test's CMake pattern
   (e.g. `test/golf_course_validate`). Skip cleanly when `GOLF_CATALOG_DIR` is unset.

## Live smoke run (the only network use in this task)

`--ids 533,589,611,605,623`, with the cache at `~/golf-catalog-cache` and export to
`/tmp/golf-catalog-smoke` (outside any git tree).
That's 5 courses × 2 API calls + about 99 images at ≥2 s spacing, roughly 4 minutes. Do
**not** run the full 442-course scrape; the coordinator runs that later. Check the §41.6
expectations for 611, and run `golf_catalog_check` on the export.

## Verification

- `~/.platformio/penv/bin/python scripts/test_baigolf_export.py` passes.
- Host build and `ctest --test-dir build/test`, all passing. On this Mac you need
  `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`
  (env only, no repo build changes). The GoogleTest download from GitHub was slow earlier
  today; if it stalls for more than 5 minutes, stop and report it rather than working
  around it.
- `GOLF_CATALOG_DIR=/tmp/golf-catalog-smoke ctest --test-dir build/test -R golf_catalog_check` passes.
- No firmware source changed: `git diff --stat main -- src lib` is empty.
- `git status` shows no scraped data (grep the diff for 苏州, 三阳 and image binaries).
- The disk is tight (about 2.5 GB free). Delete `build/test` when done. Firmware builds
  are not needed.

## Commits (local only; do not push)

1. `docs: CONTRACTS-V2 §41 course catalog from baigolf` (the §41 text, the design doc,
   this brief).
2. `feat: baigolf catalog scraper and exporter (§41)` (scripts, fixtures, tests, CTest).

Commit messages must not carry assistant attribution trailers. Report worker_done with
test counts, smoke-run results for all five courses (nines, tees, rating, map count,
exported name), anything in `export-report.txt`, and any §41 rule that did not fit the
real data. If a rule is wrong for the real data, stop and ask rather than inventing a new
rule.
