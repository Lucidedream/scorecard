# Scorecard v2 — contract changes

Supersedes the named sections of `CONTRACTS.md`. Everything not restated here still
applies. Written 2026-08-29 after the first successful on-device round.

## 1. The scoring model is inverted

**Old:** `strokes` entered; `putts` and `in100` were disjoint subsets of it;
`long = strokes - putts - in100`.

**New:** three entered fields in a fixed order, and the total is derived.

| Order | Field | Meaning |
| --- | --- | --- |
| 1 | `putts` | Strokes on the green |
| 2 | `in100` | **All** strokes played from inside 100 yards, **putts included** |
| 3 | `out100` | Strokes played from outside 100 yards — the shots that got you into the scoring zone |
| — | `strokes` | **Derived, never stored:** `in100 + out100` |

### Invariants

```
putts   <= in100
strokes == in100 + out100          (derived)
```

`in100` **carries over from putts**: raising `putts` to *n* raises `in100` to at least
*n* automatically. Lowering `in100` below `putts` lowers `putts` to match. The two move
together at the floor; they are not independent.

### Why this is better

You never compute your own total. On the green you know your putts; walking off you know
your wedge count; you know how many shots it took to get there. Each number is one you
actually observed, and the score falls out. The old model asked for the total *and* its
parts, which is one more thing to get wrong.

### The stat buckets are unchanged

```
long game  = out100
short game = in100 - putts
putting    = putts
total      = out100 + in100
```

Same three disjoint buckets as `CONTRACTS.md` §2, derived differently. Nothing in the
stats story changes.

### Not-entered sentinel

A hole is entered iff `in100 + out100 > 0` — that is, derived `strokes > 0`. This still
works because a hole cannot legitimately take zero strokes. Note `out100 == 0` alone is
**valid**: an ace on a sub-100-yard par 3 has `out100 = 0, in100 = 1, putts = 0`.

### Pre-seeding

`strokes` is derived, so it can no longer be seeded to par directly. Seed the parts
instead, on an unentered hole only: `putts = 2`, `in100 = 2`, `out100 = par - 2`, which
reconstructs par exactly. Setting *Start hole at par*, default on. The hole stays
unentered until a counter is actually touched.

## 2. File format v2

`"v": 2`. Store `putts`, `in100`, `out100`. **Do not store `strokes`** — it is derived
(same rule as long game in `CONTRACTS.md` §1).

`in100` changes meaning between v1 and v2 (it now includes putts), so a v1 file cannot
be read as v2. **Reject `"v": 1` files with a clear log line** rather than migrating:
the only v1 data in existence is a handful of test rounds, and a wrong migration would
silently misreport them.

`index.csv` gains an `out100` column; its `strokes` column keeps the derived total.

## 3. Round dates are removed from the entry flow

Entering a date on seven buttons is not worth it, and the X4 has no RTC.

* Do not prompt for a date at setup.
* `dateYmd = 0` means unknown. Write `"date": null` in round files and leave the
  `index.csv` date cell empty.
* **Round filenames become sequence-based:** `round-NNNN-<slug>.json`, zero-padded to
  four digits, where `NNNN` is one greater than the highest sequence already present in
  `/golf/rounds/`. This replaces the date-based scheme in `CONTRACTS.md` §5.5.
* Keep the existence check and the `-2` suffix as a safety net even though sequence
  numbers should be unique by construction. Never overwrite a completed round.
* Keep `dateYmd` in the struct and the schema. A later milestone may set it from NTP
  when Wi-Fi is available; nothing should have to change shape when it does.

## 4. Product changes

* The app is called **Scorecard**, not Golf. `GolfStrings::APP_TITLE` becomes
  `"Scorecard"`. Internal type and file names stay `Golf*` — renaming them would churn
  the whole tree for no user-visible gain.
* Its row moves to the **top** of the home menu, above File Browser.
* It needs its own icon rather than borrowing `Bookmark`. A 1-bpp icon in the existing
  icon format, legible at menu size on e-ink: a flag on a green, or a ball on a tee.
* **Two or three courses ship inside the firmware** as `static constexpr` data in flash,
  so the app is usable with an empty SD card. SD-loaded courses from `/golf/courses/`
  are listed alongside them; an SD file with the same name takes precedence, so a
  built-in can be corrected without a rebuild. Budget a few hundred bytes each — the
  app currently has 1.06 MB of flash headroom.


## 5. Reading screens (M2)

### 5.1 The scorecard table

Rows, in this order — it mirrors the entry order so the card reads the way it was filled:

| Row | Source | Notes |
| --- | --- | --- |
| Hole | 1..holeCount | header |
| Par | `par[i]` | omitted entirely on a par-free course |
| Score | `in100 + out100` | derived, bold |
| Putts | `putts[i]` | |
| In 100 | `in100[i]` | includes putts, per §1 |
| Out 100 | `out100[i]` | the long game |

Unentered holes render as `·`, never `0`. An 18-hole round splits Front / Back with OUT
/ IN / TOTAL columns; a 9-hole round is one table with a TOTAL column and no tabs.

**On a par-free course, drop the Par row and every to-par figure** rather than printing
zeros. `golfHasPar()` already reports this. A card that shows "+72" because par is
unknown is worse than one that shows no par at all.

### 5.2 History

Reads `/golf/rounds/index.csv` and **nothing else**. Parsing round JSON files to render
a list is forbidden — it was too slow in v1 and is still too slow.

* **Newest first.** Rows are appended chronologically, so the file order must be
  reversed for display.
* **Bounded at 50 rows in RAM.** Stream the file once through a fixed ring buffer of the
  most recent 50 entries; never accumulate the whole file. At ~56 bytes a row that is
  under 3 KB. If more rounds exist, say so on screen rather than silently truncating.
* Dates are unknown in v2 (§3), so a row shows **course, score, and to-par** — not a
  date. Suppress to-par when the archived `par` total is 0.
* A malformed row is skipped with a log line; it must never abort the listing.

### 5.3 Round summary

Selecting a History row shows a summary built **entirely from that CSV row**: score,
to-par, putts, inside-100, and long game (`strokes - in100`). No JSON parsing.

This is deliberate. Every figure worth showing at the summary level is already in the
index, so the hole-by-hole view — which does need the round file — can wait for M3
without making History useless in the meantime.


## 6. Commit-and-advance on Confirm (v2.2)

*Added 2026-08-29 at the owner's request, after playing with the build.*

### The problem

Pre-seeding to par (§1) means a hole played to par needs **zero** counter presses — the
displayed values are already right. But a hole stays *unlogged* until a counter is
actually mutated, so pressing Confirm three times to review the fields and walking on
leaves the hole blank. The cheapest hole to score is the one most likely to be lost.

### The rule

On a hole that is **not yet logged**, the **fourth consecutive Confirm** — equivalently,
the first Confirm after focus has completed one full lap back to its starting field —
**commits the displayed values and advances to the next hole.**

```
unlogged hole, focus starts at Putts
  Confirm 1 -> In100
  Confirm 2 -> Out100
  Confirm 3 -> Putts        (one full lap; all three fields reviewed)
  Confirm 4 -> COMMIT + advance to next hole
```

On a hole that **is already logged**, Confirm only cycles focus. It never advances. This
is what makes going back to correct a hole safe: you can traverse its fields freely
without being pushed forward.

### Required conditions and resets

* **Nothing to commit means no commit and no advance.** If all three counters are zero —
  a par-free course, or *Start hole at par* switched off — the fourth Confirm just keeps
  cycling. Committing zeros would store a hole that `isEntered` still reads as unlogged
  while having skipped past it, which would silently lose holes on a par-free round.
* **Mutating any counter logs the hole immediately** (existing behaviour) and from that
  point Confirm only cycles. The lap counter is irrelevant once logged.
* **The lap counter resets on every hole change**, by either direction, and on commit.
* **Advance uses the same wrap as the Right button** — hole 18 wraps to hole 1. Keeping
  one advance rule is worth more than special-casing the last hole; the user has already
  learned that wrap from Right.

### Why this shape

It costs nothing to learn: reviewing all three fields is what a golfer does anyway when
the hole was a par, and the commit falls out of one extra press at the moment they would
otherwise walk away. It also cannot fire by accident on a hole already scored, which is
the case where an unexpected advance would be genuinely annoying.

## 7. Course list ordering (supersedes CONTRACTS.md §10)

Built-in courses appear **first, in table order**, followed by SD-card courses sorted
alphabetically, case-insensitive.

CONTRACTS.md §10 previously specified alphabetical ordering across everything. That was
my rule and it is now overridden: the owner wants his home course first, and a fixed
table order for built-ins expresses intent that an alphabetical sort cannot. Built-in
table order is therefore meaningful and must be preserved rather than re-sorted.

### 7.1 An SD override keeps the built-in's position

*Ruling 2026-08-29, after v2.2 correctly flagged the conflict rather than guessing.*

§4 lets an SD file override a same-named built-in so a course can be corrected without
a rebuild. §7 puts built-ins first in table order. These collided: an override was
emitted with `builtInIndex = -1`, so a corrected `Sanyang Golf Club.json` would sort
alphabetically among SD courses and land *after* Pebble Beach.

**An SD file that overrides a built-in inherits that built-in's list position.**
Correcting a course must not demote it. The whole point of the override is to fix data
in place, and a side effect that moves the owner's home course down the list defeats
it. `CourseStore::enumerate()` must carry `builtInIndex` through overrides so the sort
can honour it.

Only SD courses that override *nothing* sort alphabetically after the built-ins.

Order: **Sanyang Golf Club**, MoganShan Gowin, Pebble Beach, Template course. `Practice 9` is removed —
it was a generic filler card that the owner does not use.


## 8. Viewing an archived round's full card (v2.4)

*Added 2026-08-29. Supersedes §5.3's "no JSON parsing" rule for the detail view only.*

### What changes and what does not

§5.2's rule is unchanged and remains absolute: **History builds its list from
`index.csv` and nothing else.** Parsing round files to render a list is still forbidden.

What changes is the *destination*. Selecting one row may open **one** round file. Opening
a single chosen file costs nothing like scanning every file, and the hole-by-hole data is
already on the card — every finished round writes complete per-hole `par`, `putts`,
`in100`, and `out100`.

### The flow

```
History (index.csv only)
  └─ select a row
       ├─ round file loads  ->  GolfCardActivity, read-only   (normal path)
       └─ load fails        ->  GolfRoundSummaryActivity      (graceful fallback)
```

The summary screen is **retained as the degradation path**, not deleted. An `index.csv`
row can outlive its round file — the file may be deleted from the SD card, truncated, or
be a rejected v1 record — and in every such case the CSV row still holds valid totals.
Showing those totals plus a plain note that the hole-by-hole detail is unavailable is far
better than an error screen, and it means a missing file can never make a round
unreadable.

### The card must take a round, not read the store

`GolfCardActivity` currently reads `GOLF_ROUND_STORE.getRound()` at four sites. It must
instead **hold its own `GolfRound` member** and populate it once in `onEnter()` — copied
from the store for a live round, or from the loaded file for an archived one. 164 bytes
on an activity that is already heap-allocated.

Copying rather than referencing also removes a live hazard: a reference into the store
could observe a round mutating underneath the render pass.

An archived card is **read-only**. No counter mutation, no commit-and-advance, no hole
navigation that writes. Tab switching and Back only.

### The reader

A round-file reader is new. It must:

* Reject `"v": 1` with a clear log line, exactly as `GolfRoundStore` does — `in100`
  changed meaning in v2 and a silent reinterpretation would misreport an old round.
* Enforce array length equals `holes`, rejecting rather than padding or truncating.
* Run the loaded round through `validateGolfRound()` and log any repair.
* Return failure — never a partial round — on a missing file, unparseable JSON, or any
  validation rejection, so the caller can fall back to the summary.

Share the parsing shape with `GolfRoundStore::fromJson` where practical; the completed
round schema (§5.2 of CONTRACTS.md) is the state schema minus `currentHole` and `yards`.


## 9. One-handed scoring (v2.5)

*Added 2026-08-29 after the owner scored a real round on the device.*

### 9.1 Power cycles the field

Confirm sits on the front face while Up/Down are the side rocker, so cycling the field
and changing a count need two hands. **The Power button also cycles the field** on the
scoring screen, putting every scoring action under one thumb.

* Power is an **addition, not a replacement** — Confirm keeps working. Removing it would
  break muscle memory for no gain and leave the screen with one input path.
* Applies **only to the scoring screen**. Everywhere else Power keeps its stock meaning.
* **A short press cycles; a hold still sleeps.** The hold path in `main.cpp` compares
  against `SETTINGS.getPowerButtonDuration()` (400 ms with the default `IGNORE`
  short-press setting), so consuming the short release cannot swallow a deliberate sleep.
  Sleep must remain reachable from the scoring screen — a golfer pocketing the device
  mid-round depends on it.
* **The screenshot combo (Power + Down) must keep working.** Do not consume Power while
  Down is held.
* If `SETTINGS.shortPwrBtn` is set to `SLEEP`, that user choice wins: the scoring screen
  must not hijack Power. Honour the setting rather than overriding it.

### 9.2 Commit on the third press, not the fourth

§6 committed on the press *after* a full lap. That is one press too many.

Focus starts on Putts, so all three fields have been seen after two presses. **The press
that would wrap focus back to the starting field commits and advances instead.**

```
unlogged hole, focus starts at Putts
  Press 1 -> In100
  Press 2 -> Out100        (all three fields now seen)
  Press 3 -> COMMIT + advance   (instead of wrapping to Putts)
```

On an unlogged hole with values to commit, focus therefore never wraps — the wrap press
becomes the commit. Every §6 condition is otherwise unchanged: a logged hole only
cycles and never advances, a hole with nothing to commit only cycles, and the counter
resets on hole change, on commit, and on any counter mutation.

Both Confirm and Power feed the same counter. Three presses in any mix — Power, Power,
Confirm — commit and advance.


## 10. Advance from the last field (v2.6) — supersedes §6 and §9.2

*Added 2026-08-29 at the owner's request, before taking the build to a round.*

### The rule

**Pressing Field while focus is on Enter Scoring Zone (`Out100`, the last field) always
advances to the next hole** — whether or not the hole was already logged, and whether or
not anything was changed.

```
focus Putts   + Field -> In100
focus In100   + Field -> Out100
focus Out100  + Field -> next hole
                         (committing on the way out if the hole was unlogged
                          and has values to commit)
```

Focus resets to `Putts` on every hole change, so a hole always takes three presses to
walk through and leave, logged or not.

### Why this is better than what it replaces

§6 and §9.2 counted presses since arriving at the hole and fired on the third. That
worked but carried real state: a counter, three reset conditions (hole change, commit,
counter mutation), and a stale-count hazard that needed the mutation reset to prevent.

This rule is **positional and stateless**. The answer depends only on which field is
focused, so `pressesSinceReset` and every reset condition can go. Delete them rather than
leaving them unused — a counter nothing reads is a trap for the next reader.

### The three outcomes

| Focus | Hole state | Result |
| --- | --- | --- |
| Putts or In100 | any | cycle focus, nothing else |
| Out100 | unlogged, has values to commit | **commit, then advance** |
| Out100 | already logged | **advance** (no commit — the score already stands) |
| Out100 | unlogged, nothing to commit | **advance** (hole stays correctly unlogged) |

That last row is deliberate. Advancing without committing is ordinary navigation — it is
exactly what the Right button already does — and the hole stays blank on the card, which
is the truthful outcome. Do not commit zeros to avoid an empty hole.

Advance uses the same wrap as Right: hole 18 wraps to hole 1.


## 11. Trends (M3)

*Added 2026-08-30. Scope corrected: the planned "CSV export" half of M3 is dropped —
`/golf` is already browsable and downloadable through the existing webserver (`/files`,
`/download`), only `System Volume Information` and `XTCache` are hidden, and
`index.csv` is already a CSV. There was nothing to build.*

### Source and bound

Trends read **`index.csv` only**, through the existing `GolfHistoryReader`. No round
JSON, no new file, no new parser. The reader already streams the most recent 50 rounds
into a fixed ring buffer, and that same window is the trend window.

Adding no new I/O is the point: every figure below is a fold over data already in RAM
when History is open.

### The figures

Over the rounds in the window, **18-hole and 9-hole rounds must not be mixed** — a 9-hole
round would halve every average. Compute over 18-hole rounds; if fewer than two exist,
say so rather than showing a figure derived from one round.

| Figure | Definition |
| --- | --- |
| Rounds | count in the window |
| Scoring average | mean `strokes` |
| Average to par | mean `strokes - par`, suppressed when any round in the window has `par == 0` |
| Best / worst | min and max `strokes` |
| Putts per round | mean `putts` |
| Bucket mix | mean `out100`, mean `in100 - putts`, mean `putts`, as counts and as percentages of `strokes` |

Integer arithmetic only, no floating point on the reader path. Averages are shown to
one decimal place and **rounded symmetrically about zero**:

```cpp
// Rounds half away from zero. C++ integer division truncates toward zero, so the
// bias term must follow the sign of the numerator or negative averages round the
// wrong way.
int32_t golfTenths(int32_t sum, int32_t n) {
  return (sum * 10 + (sum >= 0 ? n / 2 : -(n / 2))) / n;
}
```

*Corrected 2026-08-30. §11 originally specified `(sum * 10 + n/2) / n`, which is only
right for non-negative sums. M3 caught it: average-to-par is signed, and for a sum of
-25 over 10 rounds that formula yields -2.4 for a true -2.5 — a round-toward-zero bias
that flatters every under-par average. Verified before ruling.*

**Use one shared helper for every average**, signed or not, rather than a signed variant
beside an unsigned one. Two formulas invite the wrong one being picked later, and the
figures most likely to be added next — a to-par trend line, a differential — are exactly
the signed ones.

### Presentation

A **Trends row on Golf home**, below History. One screen, no tabs.

* **Fewer than two 18-hole rounds:** a plain message saying trends need at least two
  rounds, and how many exist. Not an error, and not an empty table of zeros.
* **Par-free rounds in the window:** score figures still compute; every to-par figure is
  suppressed, exactly as §5.2 does for a par-free History row.
* Show the window size actually used, so "average over 4 rounds" is never mistaken for a
  lifetime average.

### What this is not

No handicap estimate. It needs a differential against course rating and slope, which no
course file carries and which the owner has not asked for. Reporting a number that looks
like a handicap but is not one is the same class of error as the fabricated course data
in v2.1 — plausible, authoritative-looking, and wrong.


## 12. Penalty tracking (v3)

*Added 2026-08-30. Replaces the on-device course editor as the next milestone. Design and
UI mocks approved by the owner.*

### 12.1 What a press does

Power cycles the field, which frees Confirm. Confirm opens a two-option sheet —
**Hazard** or **OB** — and picking one does three things atomically:

1. Adds **one shot to the focused field** (the swing that was actually played).
2. Adds the **penalty strokes**: Hazard `+1`, OB `+2`.
3. Appends a **marker** (`H` or `OB`) to that field, in order.

OB costs two because this follows **USGA Local Rule E-5** — two penalty strokes and a
drop, taken instead of stroke-and-distance. It is the standard casual-play local rule, so
the device agrees with what a group actually scores.

**The picker never guesses a bucket.** It cannot know whether the swing came from the tee
or from 80 yards, so the golfer puts the shot in the right field and the picker only adds
the penalty. This is the same principle that keeps the three-bucket split truthful.

### 12.2 Stroke arithmetic

```
strokes        = in100 + out100 + penaltyStrokes
penaltyStrokes = (hazards x 1) + (OBs x 2)
```

The buckets stay disjoint and gain a fourth:

```
long game  = out100
short game = in100 - putts
putting    = putts
penalties  = penaltyStrokes
```

This is a correctness improvement, not just a feature: penalties currently hide inside
whichever field they were counted in and silently inflate it.

`isEntered` remains `in100 + out100 != 0`. A hole whose only event is a penalty cannot
occur — every penalty also adds a shot to a field.

### 12.3 Storage

```cpp
static constexpr uint8_t MAX_PENALTIES_PER_HOLE = 8;

// One nibble per event, two events per byte, in the order they happened.
//   bits 0-1  field  (0 = putts, 1 = in100, 2 = out100)
//   bit  2    kind   (0 = hazard, 1 = OB)
//   bit  3    reserved, must be written 0
uint8_t penaltyCount[MAX_HOLES];                               // 18 bytes
uint8_t penaltyEvents[MAX_HOLES][MAX_PENALTIES_PER_HOLE / 2];  // 72 bytes
```

`GolfRound` grows 164 -> 254 bytes. RAM is not the constraint; the packing exists because
the round is serialised to `state.json` on every flush and a compact struct keeps that
write small.

### 12.4 Add and remove

* **Add** appends at `penaltyCount[hole]`, increments the field, and increments the count.
* **At the cap (8)** the picker still opens but reports the hole is full. It must **never**
  silently drop an event.
* **Remove** is the exact inverse: `Down` on a field that has markers removes that field's
  **most recent** marker, along with its shot and its penalty strokes. `Down` on a field
  with no markers behaves exactly as it does today.
* Removal must not disturb the order of the remaining events on other fields.

### 12.5 File format v3

`"v": 3`. Round files and `state.json` gain a `penalties` array per hole of
`[field, kind]` pairs, in order. `index.csv` gains `hazards` and `obs` columns so trends
remain a fold over the index with no round files opened.

**v2 files are READ, not rejected.** Unlike the v1 -> v2 break, no existing field changes
meaning — a v2 round simply has no penalties. Load it with zero penalties and upgrade on
next write. Rejecting rounds the owner has already played would be gratuitous.

v1 files stay rejected, as before.

### 12.6 Button safety

**Confirm takes the penalty role only while Power can cycle the field.** Power does not
cycle when `SETTINGS.shortPwrBtn == SLEEP` (nor on X4 Pro under `PWR_CONFIRM`). If Confirm
were bound unconditionally, that setting would leave **no field control at all** and the
scoring screen would be stuck.

When Power cannot cycle: Confirm keeps cycling the field, and the picker moves to a
Confirm long-press. **The scoring screen must never reach a state with no field control.**

**Confirm never advances the hole *while it is the penalty button*.** Advance belongs to
whichever button is currently cycling the field (§10). A penalty is not a reason to leave
a hole — it is usually added mid-hole with shots still to come.

*Clarified 2026-08-30. P2 correctly read the original absolute wording as a contradiction
and asked. In SLEEP mode Confirm **is** the field-cycle button, so it must carry the full
§10 positional logic including advance-and-commit from Out100. Without that, a hole
played to par could never be committed by cycling in that mode — the golfer would have to
touch a counter to log it, which is exactly the lost-hole problem §6 and §10 were written
to fix. The clause was only ever meant to stop the **penalty** button from advancing.*

**Advance follows the field button, not a named button.** State it that way in code and in
future contract text; naming Power invites this same contradiction the next time the
binding moves.


### 12.7 Migrating an existing index.csv

*Ruling 2026-08-30, after P1 correctly identified that §12.5 and CONTRACTS.md §5.3
conflict and refused to guess.*

§5.3 says `index.csv` is appended to and never rewritten wholesale. §12.5 gives it two
new columns. An existing v2 index has a 9-column header, so appending 11-column v3 rows
produces a file whose header does not describe its rows.

**Do the one-time migration.** A CSV whose header disagrees with its rows is a silent
data-corruption trap: the owner downloads it, opens it in a spreadsheet, and every column
after `putts` is misread with nothing on screen to indicate it. That is worse than the
risk §5.3 was written to prevent.

§5.3's actual concern was **losing rounds to a failed rewrite**, not rewriting as such. So
migrate, but never with the live file as the only copy:

1. Detect a v2 header on open.
2. Write the full migrated content to `index.csv.new`, with the v3 header and every
   existing row widened with empty `hazards` and `obs` fields.
3. **Verify** the new file parses and yields the same row count as the original.
4. Rename `index.csv` to `index.csv.bak`, then `index.csv.new` to `index.csv`.
5. Delete `index.csv.bak` only after step 4 succeeds.

If any step fails, leave the original in place and log it. A failed migration must be a
no-op, never a partial file.

Empty `hazards`/`obs` on a migrated row means *not recorded*, which is truthful — those
rounds were played before penalties were tracked. It does not mean zero, and trends must
not treat it as zero. **Keep the reader's mixed v2/v3 row tolerance**; it is what makes a
failed or skipped migration harmless.

Now is the cheapest possible moment to do this: the owner has very few archived rounds.


### 12.8 Penalties are not a fourth row in the trends mix

*Ruling 2026-08-30, after P3 identified a denominator mismatch the design mock did not
account for. The mock shows penalties as a fourth bucket; that is wrong and the mock is
superseded here.*

The 'Where the shots go' mix presents its rows as shares of one whole. Long, short and
putting fold over **all** 18-hole rounds. Penalty figures fold over only the rounds with
`penaltiesRecorded` (§12.5). Putting them in one table implies a shared denominator that
does not exist: with 6 rounds of which 2 recorded penalties, dividing a 2-round penalty
average by a 6-round scoring average produces a number whose numerator and denominator
describe different populations.

**Therefore:**

* The mix stays **three rows** — long, short, putting — over all 18-hole rounds.
* Penalties appear as their **own figure outside the mix**: penalties per round with the
  hazard and OB split, **no percentage**.
* That figure is **labelled with its round count**, so it reads visibly as a different
  population from the figures above it.

Note what this exposed: since §12.2 made `strokes = in100 + out100 + penaltyStrokes`, the
three existing rows now sum to `strokes - penalties` rather than to 100%. The gap is the
penalty share — but the table is only coherent when every row spans the same rounds.

**When this becomes easy:** once every round in the window carries penalty data,
`penaltyRounds == rounds`, the denominators converge, and a four-row mix summing to 100%
is both correct and trivial. Build it then. Do not build a conditional that changes the
screen's shape based on data the owner cannot see.


## 13. Fixes from the second on-device round (v3.1)

### 13.1 Every mutation path must seed (BUG — data loss)

`mutateCounter()` calls `seedGolfHoleAtPar()` before mutating; `applyPenaltyPick()` and
`removeOrDecrement()` do not. On an unlogged hole the screen *displays* the pre-seeded par
preview, but those values are not stored until something seeds them. Adding a penalty
therefore makes the hole entered, the display switches from preview to real stored values,
and the golfer's putts and inside-100 appear to be wiped.

**Rule: seeding is a precondition of every path that mutates a hole, not a feature of one
of them.** Any code that changes a counter, appends a penalty, or removes a penalty on a
possibly-unlogged hole must seed first, exactly as `mutateCounter()` does.

Put the seed inside a single shared helper that all three paths call. A rule enforced by
remembering to call something in three places is a rule that will break again the next
time a fourth path is added.

### 13.2 The footer describes the four front buttons only

The scoring footer showed five cells for four front buttons. The X4 has four front
buttons (Back, Confirm, Left, Right), two side buttons (Up/Down), and Power. **Only the
four front buttons get footer cells** — the side rocker and Power are found by feel, not
by reading.

Scoring screen footer, in this order, matching the physical layout:

| Cell | Button | Label |
| --- | --- | --- |
| 1 | Back | Menu |
| 2 | Confirm | Penalty |
| 3 | Left | Prev |
| 4 | Right | Next |

Count (Up/Down) and Field (Power) are removed from the footer. They are not front buttons.

### 13.3 The picker footer shows two cells

Same rule. The penalty sheet showed three cells (Cancel / Choose / Add); Up/Down is the
side rocker and does not belong there. Two cells:

| Cell | Button | Label |
| --- | --- | --- |
| 1 | Back | Back |
| 2 | Confirm | Confirm |

*All three of these come from the owner's second round on the device. §13.2 and §13.3 are
one principle stated twice: a footer cell is a promise about a front button.*


## 14. Scorecard at the top of home (v3.2)

*Owner request, 2026-08-30: "the current top menu is recent opened book, I need to toggle
down to select scorecard."*

### The layout

Scorecard becomes its own row at the **very top** of the home screen, above the recent-book
cover tile, and is **selected on entry**.

```
  [ Scorecard ]        <- own row, selector index 0, selected on entry
  [ cover tile  ]      <- recent book(s), shifted down
  [ Browse files ]     <- the rest of the menu, unchanged order
  [ Recent books ]
  [ File transfer]
  [ Settings     ]
```

### The index mapping

Scorecard leaves the menu list entirely. `getScorecardMenuIndex()` and its interleaving go
away — the row is no longer *in* the menu, it is above it. The selector becomes:

| Selector index | Meaning |
| --- | --- |
| `0` | Scorecard |
| `1 .. recentBooks.size()` | recent book at `selectorIndex - 1` |
| beyond | menu item at `selectorIndex - recentBooks.size() - 1` |

This is simpler than what it replaces: one unconditional offset instead of a
theme-dependent interleave. **Both `loop()` and `render()` must derive from this one
mapping** — the earlier off-by-one bugs in this file came from two sites computing the
same position differently.

`getMenuItemCount()` still counts Scorecard, so navigation wraps correctly.

### Selection on entry

`selectorIndex` starts at `0`. Preserve the existing `initialMenuItem` behaviour: when
home is re-entered targeting a specific menu item, that still wins — only the default
changes.

### The cost, accepted

This restructures upstream's home layout inside `HomeActivity.cpp`, which is a touchpoint.
The diff there grows and will conflict on future rebases. Accepted deliberately: on this
device the scorecard is the primary application, and reaching it should not cost a
keypress every round. Keep the change as contained as possible — shift geometry, do not
reorganise unrelated drawing.
