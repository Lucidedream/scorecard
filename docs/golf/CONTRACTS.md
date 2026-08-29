# Golf Scorecard — Hard Contracts

Interfaces, formats, and invariants that **workers must implement exactly as written**.
These exist so independently-built pieces fit together and so files written by one
milestone stay readable by the next.

Changing anything in this document requires architect sign-off. If a task seems to
require a deviation, stop and report it.

## 1. The in-memory round

`src/golf/GolfRound.h`

```cpp
#pragma once

#include <cstdint>

struct GolfRound {
  static constexpr uint8_t MAX_HOLES = 18;

  char     courseName[40];
  char     tees[12];
  uint16_t dateYmd;               // (year-2000)<<9 | month<<5 | day
  uint8_t  holeCount;             // 9 or 18
  uint8_t  currentHole;           // 0-based index into the arrays

  uint8_t  par[MAX_HOLES];
  uint16_t yards[MAX_HOLES];
  uint8_t  strokes[MAX_HOLES];    // 0 == hole not entered
  uint8_t  putts[MAX_HOLES];
  uint8_t  in100[MAX_HOLES];
};
```

Rules:

* **Plain aggregate, no heap.** No `std::string`, no `std::vector`, no virtuals. It is
  passed by reference, never by value into a function that stores it.
* **`strokes[i] == 0` is the not-entered sentinel.** A hole can never legitimately take
  zero strokes, so there is no parallel "entered" array. Any code asking "has this hole
  been scored?" asks `strokes[i] != 0`.
* **Long game is never stored.** It is computed at the point of display:
  `long = strokes - putts - in100`.
* Arrays are always `MAX_HOLES` long regardless of `holeCount`. Entries at or beyond
  `holeCount` are zero and must be ignored, not rendered.

## 2. The invariant

```
For every hole i:   putts[i] + in100[i] <= strokes[i]
```

This holds at **every moment**, not merely at save time. It is enforced by the mutation
helpers, which are the only sanctioned way to change a counter:

| Operation | Behaviour |
| --- | --- |
| Increment `putts` or `in100` when `putts + in100 == strokes` | **Also increments `strokes`.** Adding a putt means a shot was taken. |
| Increment `strokes` | Always allowed, up to a ceiling of 99. |
| Decrement `strokes` below `putts + in100` | **Refused.** Returns a "blocked" result naming **every** field jointly holding the floor, so the UI can flash them inverse for one frame. |
| Decrement any counter below 0 | Refused, no-op. |
| Increment any counter on an unentered hole | Marks the hole entered (`strokes` becomes non-zero). |

Workers implement these as pure functions over `GolfRound` in `src/golf/GolfStats.{h,cpp}`
or a dedicated mutation header, **not** inline inside the activity. They must be
unit-testable without a display.

### 2.1 The mutation result type

*Amended 2026-08-29 in response to a worker question during M0. The original text said
the result named "the field that holds the floor", singular. When `putts` and `in100`
are both positive and `strokes == putts + in100`, they hold it jointly, and naming only
one of them reports something untrue. There is no correct priority order to pick, so
the result carries a set instead.*

```cpp
struct GolfMutationResult {
  bool     changed;            // a counter actually moved
  bool     blocked;            // refused by the invariant (not by a plain 0/99 clamp)
  uint8_t  blockingFields;     // bitmask of (1u << static_cast<uint8_t>(GolfField))
  bool     autoBumpedStrokes;  // a putts/in100 increment also raised strokes
};
```

Rules for `blockingFields`:

* Set a bit for **every** field that is jointly holding the floor. With
  `strokes == putts + in100` and both positive, both `Putts` and `In100` bits are set —
  reducing either one unblocks the decrement, so both are genuinely actionable.
* If only one of `putts` / `in100` is positive, only that bit is set.
* `blockingFields` is `0` whenever `blocked` is false.
* A plain clamp is **not** a block: decrementing a counter that is already `0`, or
  incrementing `strokes` at the 99 ceiling, returns `changed = false`, `blocked = false`,
  `blockingFields = 0`. Only the invariant sets `blocked`.

`autoBumpedStrokes` lets the scoring screen show why the strokes figure moved when the
user was pressing Up on putts. It is not optional — M1 needs it.

## 3. Field focus

```cpp
enum class GolfField : uint8_t { Strokes = 0, Putts = 1, In100 = 2 };
```

Confirm cycles `Strokes -> Putts -> In100 -> Strokes`. Focus resets to `Strokes` on
every hole change.

## 4. Pre-seeding to par

On first arrival at a hole where `strokes[i] == 0`, the scoring screen **displays** the
hole's par in a visually lighter/outlined style. The hole stays unentered until a
counter is actually mutated. Walking past a hole without pressing anything must leave
it blank on the card, never a silent par.

This is a setting, `Start hole at par`, default **on**.

## 5. On-disk layout

```
/golf/
  state.json                       open round; PersistableStore<GolfRoundStore>
  courses/
    <slug>.json
  rounds/
    index.csv                      one row per round; History's only input
    <YYYY-MM-DD>-<slug>.json
  export/
    holes.csv                      generated on demand
```

`<slug>` is the course name lowercased, non-alphanumerics collapsed to single hyphens,
trimmed of leading/trailing hyphens, truncated to 40 characters.
**"Alphanumeric" means ASCII `[A-Za-z0-9]` only** — every non-ASCII byte is a
separator, so `Crème Brûlée` slugs to `cr-me-br-l-e`. Transliteration would need a
mapping table costing flash for a filename nobody reads; the course's real name is
preserved in the file's `name` field and is what the UI displays. Two courses that
slug identically are separated by the §5.5 collision suffix, so nothing is lost.

### 5.1 Course file

```json
{
  "v": 1,
  "name": "Pebble Beach",
  "tees": "Blue",
  "holes": 18,
  "par":   [4,5,4,4,3,5,3,4,4, 4,4,3,4,5,4,4,3,5],
  "yards": [380,502,390,331,166,506,109,418,481,
            446,373,201,392,573,396,401,208,543],
  "si":    [7,11,3,15,17,9,13,1,5, 6,12,16,4,2,8,10,18,14]
}
```

`yards` and `si` are optional; `name`, `holes`, and `par` are required. `par` must have
exactly `holes` entries. A file failing validation is skipped with a log line, never
crashes the list.

### 5.2 Completed round file

```json
{
  "v": 1,
  "date": "2026-08-29",
  "course": "Pebble Beach",
  "tees": "Blue",
  "holes": 18,
  "par":     [4,5,4,4,3,5,3,4,4, 4,4,3,4,5,4,4,3,5],
  "strokes": [4,6,4,4,3,7,5,5,4, 5,4,4,5,6,4,5,3,6],
  "putts":   [2,2,1,2,2,3,2,2,1, 2,2,2,2,2,1,2,1,2],
  "in100":   [1,2,1,1,0,2,1,1,1, 1,1,1,2,1,1,1,1,2]
}
```

Par is copied into the round file so a round stays readable after its course file is
edited or deleted. Unentered holes are written as `0` in `strokes`.

### 5.3 History index

```
date,course,holes,strokes,par,putts,in100,file
2026-08-29,Pebble Beach,18,86,72,33,21,2026-08-29-pebble-beach.json
```

* Header row always present.
* Appended to on finish; never rewritten wholesale during normal operation.
* Course names containing a comma or quote are quoted per RFC 4180.
* `strokes` / `putts` / `in100` are round totals over entered holes only.
* **The History screen reads this file and nothing else.** Parsing every round JSON to
  render a list is explicitly forbidden — it is too slow on the C3.

### 5.4 The in-progress round file

*Added 2026-08-29. M1a correctly reported that §5 named `/golf/state.json` but never
gave its schema. The shape below is ratified as the contract.*

```json
{
  "v": 1,
  "date": "2026-08-29",
  "course": "Pebble Beach",
  "tees": "Blue",
  "holes": 18,
  "currentHole": 6,
  "par":     [4,5,4,4,3,5,3,4,4, 4,4,3,4,5,4,4,3,5],
  "yards":   [380,502,390,331,166,506,109,418,481,
              446,373,201,392,573,396,401,208,543],
  "strokes": [4,6,4,4,3,7,5,0,0, 0,0,0,0,0,0,0,0,0],
  "putts":   [2,2,1,2,2,3,2,0,0, 0,0,0,0,0,0,0,0,0],
  "in100":   [1,2,1,1,0,2,1,0,0, 0,0,0,0,0,0,0,0,0]
}
```

* It carries everything needed to reconstruct `GolfRound`, because resume-on-wake (§7)
  has nothing else to work from.
* `date` is `YYYY-MM-DD` on disk, matching §5.2, and unpacks into the in-memory
  `dateYmd`. Files stay human-readable; that is deliberate.
* **Every array has exactly `holes` entries.** A length that disagrees with `holes` is a
  corrupt file and is rejected under §9, not silently padded or truncated.
* `currentHole` is 0-based.
* Unentered holes are written as `0` in `strokes`, per the §1 sentinel.

### 5.5 Slug fallback and filename collisions

*Added 2026-08-29. The empty-slug case was reported by M1a; the collision case is one I
found while ruling on it, and it is the more serious of the two.*

**Empty slug.** A course name with no alphanumeric characters slugs to the empty string,
which would yield `2026-08-29-.json`. Substitute the literal `course`, giving
`2026-08-29-course.json`. A trailing-hyphen filename is a smell and reads as a bug.

**Collisions are real and must never overwrite.** Two rounds at the same course on the
same date is ordinary — 36 holes in a day, or replaying a course after a bad front nine.
Since the X4 has no RTC (§3), no time component is available to disambiguate. So:

* Before writing, check whether the target filename exists.
* If it does, append `-2`, then `-3`, and so on, until an unused name is found:
  `2026-08-29-pebble-beach-2.json`.
* The `file` column in `index.csv` records the name actually written.

**Silently overwriting a completed round is the worst failure this app can have** — it
destroys a record the golfer cannot reconstruct. Rank it above any concern about
filename tidiness.

## 6. Persistence triggers

`/golf/state.json` is written on a dirty flag plus exactly these four triggers:

1. Hole change (Left/Right).
2. 5 seconds idle after the last counter change.
3. **Before deep sleep**, from a guarded flush in `main.cpp`'s `enterDeepSleep()`,
   placed beside the existing `APP_STATE.saveToFile()` call. Covers both sleep paths,
   since auto-sleep timeout and the power-button hold both route through
   `enterDeepSleep()`.
4. Before deliberately navigating away from the scoring screen — the activity flushes
   itself, outside any lock, before calling `finish()` or `replaceActivity()`.
5. Finish round — writes the round file and appends the index row, then clears
   `state.json`.

**`onExit()` must not perform SD I/O.** `ActivityManager::exitActivity()` calls
`onExit()` while holding the global `RenderLock` (`ActivityManager.cpp:206-210`, whose
own comment states the lock must be held by the caller), and `AGENTS.md` forbids holding
that lock across a blocking call. *Corrected 2026-08-29: trigger 3 originally named
`onExit()`. That was wrong — it forced a choice between violating the lock rule and
losing the round. M1b-2 found the conflict. Upstream already solves this exact problem
the same way: `enterDeepSleep()` calls `APP_STATE.saveToFile()` before `goToSleep()`,
which is the idiom to follow.*

An SD write on every button press is forbidden: it is slow and it is needless wear.
Worst-case data loss is the current hole's counters, which is accepted.

### 6.1 Archive completion and retry

*Added 2026-08-29. M1a correctly reported that §6 defined the finish-round sequence but
never its failure semantics. This is a contract gap, not an implementation defect.*

Finishing a round is three writes that cannot be made atomic on an SD card:

1. Write the round file to `/golf/rounds/<name>.json`.
2. Append its row to `index.csv`.
3. Clear `/golf/state.json`.

If step 3 fails, the round is safely archived but the open round still exists on disk. A
caller that blindly retries would write a *second* round file under a `-2` suffix and a
second index row — a duplicate round.

**Rules:**

* Before deleting `state.json`, write the field `"archivedAs": "<filename>"` into it.
  That field is the commit marker.
* **Resume (§7) must treat a `state.json` carrying `archivedAs` as a completed round.**
  Delete the file, do not resume into it, and do not archive it again.
* **The marker takes precedence over every other field.** A loader must check
  `archivedAs` *before* parsing or validating the round's own fields. Otherwise
  corruption anywhere in the score arrays makes the load fail, hiding the marker, and the
  round gets archived a second time or resumed as if unfinished. Ratified 2026-08-29
  after M1b-1 identified it — the original §6.1 text left the ordering implicit.
* **Never auto-retry `archive()` after the round file has been written.** Surface the
  failure to the user instead. A retry that finds `archivedAs` already set must skip
  straight to deleting `state.json`.

**The bias is deliberate: prefer a duplicate round over a lost one.** A duplicate is
recoverable — delete a file, drop a CSV row. A round the golfer already played and
cannot reconstruct from memory is gone for good. Where the two risks conflict, protect
against loss.

Residual, accepted: a crash between steps 1 and 2 leaves a round file with no index row,
so it will not appear in History. The round data itself survives on the card. A future
milestone may add an index rebuild that scans `/golf/rounds/`; it is not required now.

## 7. Resume on wake

Deep sleep destroys RAM; waking is a cold boot through `BootActivity`. When
`/golf/state.json` holds an open round, the device must resume **directly into the
scoring screen at `currentHole`** — not CrossPoint's home, not the golf menu.

This is the entire justification for the `BootActivity.cpp` touchpoint in
ARCHITECTURE.md §2.

## 8. Derived figures

All computed by pure functions in `src/golf/GolfStats.{h,cpp}`, none stored:

| Figure | Definition |
| --- | --- |
| Long game (hole) | `strokes - putts - in100` |
| Score | sum of `strokes` over entered holes |
| To par | `score - sum(par over entered holes)` |
| Thru | count of entered holes |
| Putts / In100 / Long totals | sums over entered holes |
| 1-putts, 3-putts | counts of `putts == 1`, `putts >= 3` over entered holes |
| Worst holes | entered holes ranked by `strokes - par` descending; see §8.1 |

Sums must be over **entered holes only**. Including unentered holes silently reports a
better score than was played, which is the one bug class that would make the whole app
untrustworthy.


### 8.1 The worst-holes API

*Added 2026-08-29. M0 correctly reported that §8 named the figure but never specified
the call shape, how many entries come back, or how ties order. The implementation's
choices are ratified here as the contract.*

```cpp
struct GolfWorstHole { uint8_t hole; int16_t toPar; };

uint8_t golfWorstHoles(const GolfRound& round, GolfWorstHole* holes, uint8_t capacity);
```

* Storage is **caller-provided**; the function never allocates. It returns the number
  of entries written, which is `min(enteredHoles, capacity)`.
* It keeps the worst `capacity` holes, ordered by `toPar` descending.
* **Ties preserve hole order** — an earlier hole ranks ahead of a later one with the
  same `toPar`. Ranking must be deterministic so a screen does not reshuffle between
  repaints.
* A null buffer or zero capacity returns 0 and writes nothing.

## 9. Validating externally-loaded rounds

*Added 2026-08-29 by the architect during M0 review. This is a gap I found in my own
contract, not something the worker got wrong — M0 has no file I/O, so nothing here was
in its scope.*

The §2 invariant is airtight **through the mutation API**: no sequence of increments and
decrements can reach a state where `putts + in100 > strokes`. It is *not* airtight
against a `GolfRound` filled in from outside — a truncated, corrupt, or hand-edited
`/golf/state.json` or round file.

A concrete failure: a round loaded with `strokes = 0`, `putts = 1` is already invalid.
Incrementing putts then sees `shortShots (1) >= strokes (0)`, bumps strokes to 1 and
putts to 2, leaving `putts (2) > strokes (1)`. The rules layer is not at fault; it is
entitled to assume a valid starting state.

**Therefore M1 must not hand deserialized data to the rules layer unvalidated.** Any
loader (`GolfRoundStore`, `RoundArchive`) must pass every round through a validation and
repair step before use:

* `holeCount` is 9 or 18; anything else rejects the file.
* `currentHole < holeCount`, else clamp to 0.
* For every hole: if `strokes == 0`, force `putts` and `in100` to 0 — an unentered hole
  cannot have short-game counts.
* For every hole: if `putts + in100 > strokes`, the record is inconsistent. Preserve
  `strokes` (the number the golfer is surest of) and reduce `in100` first, then `putts`,
  until the invariant holds.
* Every repair emits a log line naming the hole and what changed. Silent repair of a
  score is not acceptable.

M1's brief will carry this as an explicit deliverable with its own test suite.


## 10. Course enumeration

*Added 2026-08-29 after M1b-1 reported that §5 never specified enumeration limits or
ordering.*

* `CourseStore` enumerates `/golf/courses/*.json` into caller-provided storage, bounded
  at **32 entries**. When more candidates exist than fit, it must report an overflow
  flag rather than truncate silently.
* Overflow is keyed on the number of **JSON candidates present**, not the number that
  validate. Reporting only valid courses would let a directory of broken files look
  empty rather than broken.
* A file that fails to parse or validate is **skipped with a log line naming the file
  and the reason**, and enumeration continues. One bad file must never hide the courses
  after it, nor abort the listing.
* **Ordering is alphabetical by course `name`, case-insensitive.** M1b-1 preserved raw
  SD directory order, which is stable enough but arbitrary; a picker whose rows appear
  in an unexplained sequence reads as broken. Sort in place over the caller's fixed
  array — an insertion sort over at most 32 entries needs no heap. This is an **M1b-2
  requirement**, not a defect in M1b-1, which was never told to order the results.
* `archivedAs` is accepted when non-empty and within the round-filename buffer; empty or
  oversized markers are rejected rather than treated as commits.
