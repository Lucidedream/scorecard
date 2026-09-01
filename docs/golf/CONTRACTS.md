# Golf Scorecard — Hard Contracts

Interfaces, formats, and invariants that **workers must implement exactly as written**.
These exist so independently-built pieces fit together and so files written by one
milestone stay readable by the next.

Changing anything in this document requires architect sign-off. If a task seems to
require a deviation, stop and report it.

## 1. The in-memory round

`src/golf/GolfRound.h`

The live model has exactly four stable player slots. Slots are never compacted or
renumbered; disabling slot 1 does not move slots 2 or 3. `CONTRACTS-V2.md` §16 defines
the corresponding v4 wire format.

```cpp
enum class TeeSelection : uint8_t { NotPlay = 0, Blue = 1, White = 2 };

struct GolfPlayerScore {
  uint8_t putts[18];
  uint8_t in100[18];
  uint8_t out100[18];
  uint8_t penaltyCount[18];
  uint8_t penaltyEvents[18][4];
};

struct GolfPlayer {
  static constexpr uint8_t NAME_CAPACITY = 24;
  char name[NAME_CAPACITY];
  TeeSelection tee;
  uint16_t yards[18];
  GolfPlayerScore score;
};

struct GolfRound {
  static constexpr uint8_t MAX_HOLES = 18;
  static constexpr uint8_t MAX_PLAYERS = 4;
  static constexpr uint8_t MAX_PENALTIES_PER_HOLE = 8;
  static constexpr uint8_t NO_PLAYER = UINT8_MAX;

  char courseName[40];
  uint16_t dateYmd;               // (year-2000)<<9 | month<<5 | day
  uint8_t holeCount;              // 9 or 18
  uint8_t currentHole;            // 0-based index into the shared hole arrays
  uint8_t currentPlayer;          // stable slot index 0..3
  uint8_t par[MAX_HOLES];         // shared course facts
  uint8_t si[MAX_HOLES];
  bool hasSi;
  GolfPlayer players[MAX_PLAYERS];
};
```

Rules:

* **Plain fixed-size aggregates, no heap.** No `std::string`, `std::vector`, virtuals,
  or per-player allocation. `GolfPlayerScore` is 144 bytes, `GolfPlayer` is 206 bytes,
  and `GolfRound` is 906 bytes; compile-time assertions lock those bounds.
* Do not create `GolfRound` as a local automatic variable on embedded task stacks. Keep
  it in the singleton store or an already heap-allocated activity and pass it by
  reference. Small `GolfPlayerScore` references are the unit of mutation.
* Slot names are round data in fixed null-terminated `char[24]` buffers. The defaults,
  installed by `initializeGolfPlayerDefaults()`, are `Noah`, `Player B`, `Player C`,
  and `Player D`.
* A slot is active **if and only if** `tee != TeeSelection::NotPlay`; there is no second
  enabled flag. The neutral `initializeGolfPlayerDefaults()` and legacy decode paths keep
  all four tees at `NotPlay`. After an explicit course selection, player setup enables P1
  on the first truthful tee described below; P2–P4 remain `NotPlay`.
* `par`, `si`, and `hasSi` belong to the course and are shared. `yards` belongs to a
  player because it follows that player's Blue or White tee.
* Handicap and net-score fields do not exist. All scores and comparisons are gross.
* Arrays are always `MAX_HOLES` long regardless of `holeCount`. Entries at or beyond
  `holeCount` are zero and ignored.

## 2. Scoring invariant and ownership

For every player and hole:

```
putts <= in100
score  = in100 + out100 + penalty strokes
entered iff in100 + out100 != 0
```

`in100` includes putts. Raising putts carries `in100` upward when needed; lowering
`in100` below putts carries putts down. A counter clamps at 0 and 99. Penalty semantics
remain those in `CONTRACTS-V2.md` §12.

Every scoring, penalty, and per-player statistics API receives an explicit
`GolfPlayerScore&` (or `const GolfPlayerScore&`). No helper infers a player from
`GolfRound::currentPlayer`; therefore a caller cannot accidentally mutate one slot while
another is selected. Helpers that also need shared par or `holeCount` receive the round
as a separate const reference.

A hole remains unentered until `in100 + out100 != 0`, even when a par preview is visible.
A penalty always adds a real field shot, so a penalty-only entered state cannot be
created through the mutation API.

## 3. Field focus and turn order

```cpp
enum class GolfField : uint8_t { Putts = 0, In100 = 1, Out100 = 2 };
```

Field focus cycles `Putts -> In100 -> Out100`; leaving `Out100` advances one flattened
turn. Turns are ordered `(hole, enabled player)` in stable slot order. The next enabled
player stays on the same hole; advancing from the last enabled player moves to the next
hole and the first enabled player. The final hole wraps to hole 0. With one enabled
player this is exactly the original one-player hole advance.

`golfFirstEnabledPlayer()`, `golfNextEnabledPlayer()`, and
`golfPreviousEnabledPlayer()` return stable slot indexes or `GolfRound::NO_PLAYER`.
`advanceGolfTurn()` owns the flattened forward rule. A completed/persisted round must
have at least one enabled slot, and `currentPlayer` must name one; setup may temporarily
hold all four slots at `NotPlay` before Complete is allowed.

## 4. Pre-seeding to par

On first arrival at a player/hole where `in100 + out100 == 0`, the scoring screen
**displays** that player's par reconstruction in a visually lighter/outlined style:
`putts = 2`, `in100 = 2`, `out100 = par - 2`. The score remains unentered until the
preview is committed or a counter/penalty mutation seeds and changes that explicit
player score. Advancing through a blank, par-free hole does not create a score.

This is a setting, `Start hole at par`, default **on**.

## 5. On-disk layout

The v1 examples retained in this section document the original files only.
`CONTRACTS-V2.md` §16.5 supersedes their singular `tees` and score arrays with the
multiplayer v4 wire shape.

```
/golf/
  state.json                       open round; PersistableStore<GolfRoundStore>
  courses/
    <slug>.json
  rounds/
    index.csv                      one row per enabled stable slot; selector/review input
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

Course selection applies this setup-only tee policy without changing neutral round or
legacy decode defaults:

1. If `CourseStore::resolveTee(..., Blue, ...)` succeeds, P1 Noah starts on Blue.
2. Otherwise, if the course's exact White row resolves, P1 starts on White.
3. An empty `tees` value with no `yards` offers both Blue and White as selection-only
   choices with zero player yardages and defaults P1 to Blue.
4. A noncanonical tee label (for example `Blue/White`) or nonzero yardage without a tee
   label resolves neither tee. P1 remains `NotPlay`, as do P2–P4.

Complete is available immediately exactly when this policy found a truthful P1 tee.
A resolver must never infer a label for yardage data or borrow a built-in alternate for
an SD override.

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

### 5.3 History index (historical v1–v3)

`CONTRACTS-V2.md` §16.6 defines the authoritative normalized v4 header, migration,
and grouped transaction rules.

```
date,course,holes,strokes,par,putts,in100,file
2026-08-29,Pebble Beach,18,86,72,33,21,2026-08-29-pebble-beach.json
```

* Header row always present.
* Appended to on finish; never rewritten wholesale during normal operation.
* Course names containing a comma or quote are quoted per RFC 4180.
* `strokes` / `putts` / `in100` are round totals over entered holes only.
* **The player selector and History list read this file and nothing else.** Parsing every
  round JSON to render either list is explicitly forbidden — it is too slow on the C3.
  Only activation of one selected History row may load that row's one round JSON.

Golf Home opens the same four-row stable-slot selector before either History or Trends.
One recovered streaming pass retains the latest name and presence bit for P1–P4. Missing
slots remain visible and disabled with the short translated value `No rounds`; logical
Next/Previous navigation skips them, and an all-missing selector still accepts Back.
Touch and Confirm recheck the live presence bit before opening a child.

The selected data activity receives an immutable slot plus a copied fallback name. It
loads only that slot and shows its identity at the right of the header; there are no
player tabs, left/right player switching, all-player name copies, or first-present
fallback inside History or Trends. Before pushing a child, the selector marks its tiny
atomic refresh flag. On return it recovers and streams again while stale rows are hidden,
then publishes the new fixed-capacity row snapshot. Deleting a slot's final round
therefore disables that row before it can be selected again.

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
5. Finish round — writes one group round file, atomically publishes all enabled-player
   index rows per `CONTRACTS-V2.md` §16.6, then clears `state.json`.

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
2. Atomically publish its 1–4 enabled-player rows to `index.csv`.
3. Clear `/golf/state.json`.

If step 3 fails, the round is safely archived but the open round still exists on disk. A
caller that blindly retries would write a *second* round file under a `-2` suffix and a
second index group — a duplicate round.

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
  straight to deleting `state.json`, before allocating archive scratch or recovering
  the history index.

**The bias is deliberate: prefer a duplicate round over a lost one.** A duplicate is
recoverable — delete a file, drop a CSV row. A round the golfer already played and
cannot reconstruct from memory is gone for good. Where the two risks conflict, protect
against loss.

Residual, accepted: a crash between steps 1 and 2 leaves a round file with no index rows,
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
| Long game (hole) | `out100` for the selected player |
| Score | sum of `in100 + out100 + penaltyStrokes` over that player's entered holes |
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

uint8_t golfWorstHoles(const GolfRound& round, const GolfPlayerScore& score,
                       GolfWorstHole* holes, uint8_t capacity);
```

* Storage is **caller-provided**; the function never allocates. It returns the number
  of entries written, which is `min(enteredHoles, capacity)`.
* It keeps the worst `capacity` holes, ordered by `toPar` descending.
* **Ties preserve hole order** — an earlier hole ranks ahead of a later one with the
  same `toPar`. Ranking must be deterministic so a screen does not reshuffle between
  repaints.
* A null buffer or zero capacity returns 0 and writes nothing.

## 9. Validating externally-loaded rounds

The §2 invariant is airtight through the mutation API but not against a truncated,
corrupt, or hand-edited file. Every state or archive loader validates before exposing a
round:

* `holeCount` is 9 or 18; anything else rejects the file.
* Every tee is one of `NotPlay`, `Blue`, or `White`; an unknown enum/token rejects it.
* At least one player is enabled. A four-`NotPlay` setup draft is not a valid persisted
  or playable round.
* `currentHole < holeCount`, else reset it to 0.
* `currentPlayer` identifies an enabled stable slot, else reset it to the first enabled
  slot.
* Validate each `GolfPlayerScore` independently. If `in100 + out100 == 0`, force putts
  to 0; otherwise reduce putts to `in100` when needed. Repair invalid/capped penalty
  records only inside the owning player's score.
* Every repair log names both the player slot and hole. Silent repair of a score is not
  acceptable.

`validateGolfPlayerScore()` returns per-hole repair masks for one explicit score.
`validateGolfRound()` returns four such result records plus cursor repairs; it cannot
collapse player results into one bitmap because that would hide which score changed.


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
