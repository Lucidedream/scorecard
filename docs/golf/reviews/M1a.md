# M1a Review — architect

Reviewed 2026-08-29. Worker: codex, dispatch `ctx_81f8ed4b71b1`, run `run_e55ef7313086`.

**Verdict: ACCEPTED.** No defects found. Five contract gaps surfaced by the worker; four
closed, one (§6.1) closed as a new requirement carried into M1b.

## Gates (architect-run, not taken from the report)

| Gate | Result |
| --- | --- |
| Golf host suites | **36/36** — GolfRules 13, GolfStats 5, golf_validate 6, golf_paths 7, golf_csv 5 |
| `pio run -e golf` | SUCCESS, 5,427,600 bytes |
| `pio run -e default` | SUCCESS, 5,427,632 bytes, no golf symbols |
| Upstream touchpoints | still 2 files: `platformio.ini` +7, `test/CMakeLists.txt` +6 |
| Pure-TU discipline | `GolfValidate` / `GolfPaths` / `GolfCsv` free of Arduino, ArduinoJson, HalStorage, heap |

## Verified by reading, not by test count

* **Repair arithmetic cannot underflow.** In the branch where `in100` is exhausted,
  `putts -= (putts - strokes)` leaves exactly `strokes`, and the branch condition
  guarantees `putts >= strokes`. Traced by hand across four cases.
* **`fromJson` validates twice** — a hole-count gate before the arrays load, then the
  real repair pass after they are populated. Had it only run first, corrupt array data
  would have loaded unrepaired. This was the most likely place for a subtle ordering bug
  and it is correct.
* **Array length is enforced** (`array.size() != count`), with type and range checks
  that were not asked for.
* **Out-of-range `currentHole` is routed through the validator** (set to `holeCount`,
  then reset to 0) rather than clamped inline, so the repair is logged instead of silent.
* **Collision loop is correct**: sequence 0 gives the base name, then 2, 3, …, with a
  `uint16_t` wrap guard, looping until the path is free.
* **Failure paths leave no debris.** A failed round-file write or a failed index append
  removes the round file, so there is never a partial round nor a round without an index
  row.

## Contract gaps surfaced (all mine)

1. `/golf/state.json` had **no schema**. Ratified as CONTRACTS §5.4, with array length
   disagreeing with `holes` tightened to a hard reject — a file that disagrees with
   itself has no trustworthy reading, and padding it would fabricate a score.
2. **Empty slug** for an all-non-alphanumeric name. Falls back to `course`.
3. **Filename collisions were unspecified** — this one I found while ruling on #2, and it
   was the serious one. Two rounds at the same course on the same date is ordinary, and
   with no RTC there is no time component to disambiguate. As written the second round
   would have destroyed the first. Now §5.5.
4. **"Alphanumeric" was undefined for non-ASCII.** Ratified as ASCII `[A-Za-z0-9]` only;
   `Crème Brûlée` slugs to `cr-me-br-l-e`. A transliteration table would cost flash for a
   filename nobody reads, and the course's real name survives in the file's `name` field.
5. **Archive completion had no failure semantics.** The three writes (round file, index
   row, clear state) cannot be atomic on SD; a blind retry after a failed clear would
   produce a duplicate round. Now §6.1: an `archivedAs` commit marker, resume treats a
   marked file as completed, and never auto-retry once the round file exists.

The bias recorded in §6.1 is deliberate: **prefer a duplicate round over a lost one.**
A duplicate is recoverable; a round the golfer played and cannot reconstruct is not.

## Known limits, recorded rather than hidden

* **The collision loop is not exercised by any test.** It depends on `HalStorage`, so it
  cannot be host-tested. `golf_paths` covers filename *generation* including suffixes;
  the loop consuming it was verified by reading only. It gets its first real exercise
  on-device at M1b. This is the highest-value thing to watch during the first
  finish-round on hardware.
* **Flash cost is still 0 bytes**, and still a link-time garbage-collection artifact —
  nothing references this code yet. The real cost arrives at M1b.
* Test target naming is inconsistent between milestones (`GolfRulesTest` vs
  `golf_validate_tests`). Cosmetic; not worth a change that touches working files.
* §6.1 postdates this milestone, so `archivedAs` is **not** implemented here. It is an
  explicit M1b deliverable.
