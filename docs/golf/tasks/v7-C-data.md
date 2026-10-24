# Task v7-C: v1 → v2 course converter and example data (CONTRACTS-V2 §37.12)

Branch: `feature/course-nines` (already checked out in this worktree). Read
CONTRACTS-V2 §37.1 (schema) and §37.12 (converter) in `docs/golf/CONTRACTS-V2.md`, plus
CONTRACTS §5 (slug rule) and §9 in `docs/golf/CONTRACTS.md`, and the current
`docs/golf/examples/README.md`.

## In scope

* `scripts/golf_convert_courses_v2.py` exactly per §37.12 (stdlib only, Python 3.9+,
  argparse, `--force`, deterministic output: 2-space indent, arrays of numbers on one line
  so files stay hand-readable like the existing examples).
* A small self-check: `scripts/golf_convert_courses_v2.py --self-test` (or a
  `test_` file next to it run with `python3 -m unittest`) covering grouping, par/si
  mismatch rejection, 9-hole input, yardless tee, label-less file, v2 passthrough, no
  overwrite without `--force`.
* Convert `docs/golf/examples/`: run the script into a temp dir, review, then replace the
  v1 per-tee files with one v2 file per course (`git rm` the old ones). Lake Hill Dragon and
  Phoenix stay **two separate courses**. Hand-write the v2 version of the deliberately
  incomplete Sanyang template so it still fails to load, and keep its explanation.
* Rewrite `docs/golf/examples/README.md` for the v2 schema and add a short "Converting
  your old files" section with the exact command (copy SD `/golf/courses` off the card,
  convert, copy back, delete the old files).
* Verify every converted file against the §37.1 rules with a throwaway check (yards per
  nine sum to the original totals, par/si identical).

## Out of scope — do not touch

Anything under `src/`, `lib/`, `test/` (another worker is changing the firmware in
parallel in this same worktree). Do not run `pio` or the CMake tests.

## Done means

Do **not** commit. Report: files added/removed, the per-course summary lines the script
printed, and the self-test result.
