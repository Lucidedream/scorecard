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

Golf Home first opens the shared fixed-capacity player selector in History mode. It
recovers `/golf/rounds/index.csv`, streams one `GolfPlayerNamesReader` pass, and renders
exactly four stable P1–P4 rows. A slot absent from the index stays visible but disabled
with the short translated value `No rounds`. Logical Next/Previous skips disabled rows;
all-absent remains Back-able. Touch and Confirm both recheck presence before activation.

The selected History list reads the same recovered index and **nothing else**. Parsing
round JSON files to render either list is forbidden — it was too slow in v1 and is still
too slow. The list receives one immutable stable slot and copied fallback name, shows
that identity on the header right, and has no player tabs or left/right player switching.

* **Newest first.** Rows are appended chronologically, so the file order must be
  reversed for display.
* **Bounded at 50 rows in RAM.** Stream the file once through a fixed ring buffer of the
  most recent 50 entries; never accumulate the whole file. On the ESP32-C3 target,
  `GolfHistoryEntry` is 84 bytes and the complete `GolfHistoryReader` is 4,472 bytes:
  4,200 bytes of entries, the 255-byte CSV line buffer, and 17 bytes of counters, flags,
  and alignment. If more rounds exist, say so on screen rather than silently truncating.
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

§5.2's rule is unchanged and remains absolute: **the player selector and selected-slot
History build their lists from `index.csv` and nothing else.** Parsing round files to
render a list is still forbidden.

What changes is the *destination*. Selecting one row may open **one** round file. Opening
a single chosen file costs nothing like scanning every file, and the hole-by-hole data is
already on the card — every finished round writes complete per-hole `par`, `putts`,
`in100`, and `out100`.

### The flow

```
Player selector (one recovered index.csv pass)
  └─ select a present stable slot
       └─ History (that slot, index.csv only)
            └─ select a row
                 ├─ round file loads  ->  GolfHistoryRoundMenuActivity
                 └─ load fails        ->  GolfRoundSummaryActivity (CSV fallback)
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
from the store for a live round, or from the loaded file for an archived one. Under the
§16 model that member is 906 bytes; the activity is already heap-allocated, so it must
remain a member and must not become an automatic local on an embedded task stack.

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

A **Trends row on Golf home**, below History. It opens the same four-row player
selector in Trends mode; choosing a present row pushes one slot-specific screen. The
screen has no player tabs or left/right switching, shows the copied Pn/name identity at
the header right, and keeps `Average over N rounds` inside the content rather than using
that header identity band.

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

At v3, `GolfRound` grew from 164 to 254 bytes. The §16 multiplayer layout supersedes
that historical size with a compile-time-bounded 906-byte aggregate. Penalty nibbles
remain packed because they are repeated for every player and serialized on every flush.

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

If a step fails before `index.csv.new` is renamed to the live path, restore or retain the
original and log it; such a pre-commit failure must be a no-op, never a partial file. The
second rename is the commit point. Failure to delete `index.csv.bak` afterward is
committed cleanup-pending state, not grounds to roll back the new live index; the next
recovery pass removes the stale backup.

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


## 14. Scorecard at the top of home (v3.2, revised 2026-08-31)

*Owner requests: "the current top menu is recent opened book, I need to toggle down to
select scorecard"; then "the second is the preview of the book. Can we hide that?"*

### The layout

The golf build uses one contiguous home menu. The recent-book cover tile and individual
recent-book selector slots are omitted. Scorecard is the first row and is selected on
entry:

```
  [ Scorecard     ]    <- selector index 0, selected on entry
  [ Browse files  ]
  [ Recent books  ]
  [ File transfer ]
  [ Settings      ]
```

The optional OPDS row keeps its upstream position after Recent books when servers are
configured. The stock build keeps its cover preview unchanged.

### The index mapping

Golf home indices map directly to the rendered rows:

| Selector index | Meaning |
| --- | --- |
| `0` | Scorecard |
| `1` and beyond | existing home menu item at `selectorIndex - 1` |

`recentBooks.size()` does not contribute selector slots or `getMenuItemCount()` in the
golf build. Recent books are still loaded for the Back-button resume shortcut and remain
available through the Recent books row, but home does not generate cover thumbnails or
allocate a cover snapshot buffer.

**Both `loop()` and `render()` must use this same direct mapping.** Touch rows begin at
`homeTopPadding` and match the single rendered menu; there is no separate cover-touch
region.

### Selection on entry

`selectorIndex` starts at `0`. Preserve the existing `initialMenuItem` behaviour: when
home is re-entered targeting a specific menu item, that still wins — only the default
changes.

### The cost, accepted

This restructures upstream's home layout inside `HomeActivity.cpp`, which is a touchpoint.
The diff there grows and will conflict on future rebases. Accepted deliberately: on this
device the scorecard is the primary application, and reaching it should not cost a
keypress every round. Keep the golf branch contained; do not alter the stock home layout.


## 15. Dates, and percentages in trends (v4)

### 15.1 Percentages return to the trends mix — supersedes §12.8

§12.8 removed the penalty percentage because the scoring fold covered **all** 18-hole
rounds while the penalty fold covered only rounds with `penaltiesRecorded`. Dividing one
by the other compared different populations.

**The fix is to make them the same population, not to drop the number.** Fold *every*
bucket in the mix over the rounds that have penalty data:

```
mix rounds = 18-hole rounds with penaltiesRecorded
long / short / putting / penalties  -> all folded over that same set
```

Now the four rows share one denominator and sum to 100%, and the percentage is
meaningful. §12.8's reasoning was right; its conclusion was too pessimistic.

**Label the mix with its own round count** — e.g. *"over 4 rounds"* — because it may
cover fewer rounds than the headline scoring average above it. That gap closes on its
own: rounds predating penalty tracking age out of the 50-round window, after which the
two counts converge permanently.

Suppress the whole mix when fewer than two rounds qualify, as §11 already does.

### 15.2 Round dates without manual entry

The owner will not type a date every round, and he should not have to.

**Ordering already works and does not depend on dates.** History is newest-first through
the ring-buffer reversal, and round files carry a monotonic `round-NNNN` sequence. Dates
are for *display*, not sort order. Do not make sorting depend on them.

**`HalClock` is a dead end on this device.** It requires a hardware RTC
(`_sdkRtc.begin()`), which the X4 does not have, so `isAvailable()` is false and
`syncFromNTP()` no-ops. Do not route through it.

**Use the ESP32's own system clock instead.** The chip has an internal RTC that ESP-IDF
keeps running across deep sleep, so a single successful sync survives sleeps and reboots
without any hardware RTC:

* On a successful Wi-Fi connection, run an SNTP sync that sets **system time**
  (`configTzTime` + `time(nullptr)`), independent of `HalClock`.
* Re-sync opportunistically whenever Wi-Fi connects — the owner already connects to
  upload courses and firmware. No sync is ever forced, and none is required to play.
* Stamp the date into the round **when it is archived**, not when it starts.

**Unknown stays unknown.** If the clock has never been set, `time(nullptr)` returns a
value near the 1970 epoch. Treat any year before 2020 as *no date* and display nothing —
the same discipline as par-free rounds and unrecorded penalties. A fabricated date is
worse than a blank.

**Accuracy is adequate and must not be overstated.** Deep-sleep timekeeping runs off an
internal RC oscillator with meaningful drift. That is fine for a date and useless for a
timestamp: display the date only, never a time of day, and re-sync whenever Wi-Fi is
available.

### 15.3 Footers stay on the theme path

Every new screen uses `mappedInput.mapLabels()` + `GUI.drawButtonHints()`, exactly as
`UiListActivity::drawFooter()` and the v3.4 scoring screen do. **No screen hand-draws
footer geometry.** Beyond matching the app's sizing, `mapLabels()` is what honours the
owner's front-button remapping — a hand-drawn footer silently ignores it.


## 16. Fixed-roster multiplayer (v4 wire format)

This section supersedes every singular-player `GolfRound`, `tees`, `yards`, score-array,
and cursor example in both contract files. Score arithmetic, penalty semantics, blank
holes, and gross-score statistics remain unchanged; they now apply independently to each
enabled player.

### 16.1 Stable slots and tee selection

A round always owns exactly four slots, numbered 0 through 3. Slots never move when a
player is disabled, so a stored score, UI selection, and archive entry always identify
the same person. Each slot has a round-specific, null-terminated UTF-8 `char[24]` name.
`initializeGolfPlayerDefaults()` installs these defaults:

| Slot | Default name |
| ---: | --- |
| 0 | Noah |
| 1 | Player B |
| 2 | Player C |
| 3 | Player D |

The domain tee type is fixed and language-independent:

```cpp
enum class TeeSelection : uint8_t { NotPlay = 0, Blue = 1, White = 2 };
```

A player is active **if and only if** the tee is not `NotPlay`. There is no stored
`enabled` flag. `initializeGolfPlayerDefaults()` remains neutral: it installs names and
leaves all four zero-valued tees at `NotPlay`, as do legacy decode defaults. After the
user selects a course, setup applies this device-feedback policy:

1. P1 Noah starts on Blue when `CourseStore::resolveTee()` resolves Blue.
2. If Blue is unavailable but exact White resolves, P1 starts on White.
3. When both `course.tees` is empty and `hasYards` is false, Blue and White are valid
   selection-only choices with zero yards and P1 defaults to Blue.
4. Noncanonical tee text and unlabeled nonzero yardage resolve neither choice; P1 stays
   `NotPlay` rather than guessing.

P2–P4 always start `NotPlay`. Complete is enabled on the first setup render whenever P1
received one of those truthful choices; otherwise the four-disabled state remains a
valid draft only. Before saving or entering scoring, `currentPlayer` is normalized to
the first enabled stable slot.

Names are copied and safely truncated at UI and persistence boundaries without splitting
a UTF-8 sequence. They are never represented by `std::string` in the round. `Blue`,
`White`, and `NotPlay` are wire/domain tokens; user-facing labels use the normal `tr()`
translation path rather than serializing translated text.

There is no player handicap and no net score. Stroke index is retained as one shared
course fact for display and round reconstruction, not as input to handicap arithmetic.

### 16.2 Fixed-memory ownership

`GolfPlayerScore` owns only one player's mutable score and packed penalty arrays.
`GolfPlayer` adds that slot's fixed name, tee, and tee-specific yards. `GolfRound` owns
shared course data plus all four players:

```cpp
struct GolfPlayerScore {
  uint8_t putts[18];
  uint8_t in100[18];
  uint8_t out100[18];
  uint8_t penaltyCount[18];
  uint8_t penaltyEvents[18][4];
};

struct GolfPlayer {
  char name[24];
  TeeSelection tee;
  uint16_t yards[18];
  GolfPlayerScore score;
};

struct GolfRound {
  char courseName[40];
  uint16_t dateYmd;
  uint8_t holeCount;
  uint8_t currentHole;
  uint8_t currentPlayer;
  uint8_t par[18];
  uint8_t si[18];
  bool hasSi;
  GolfPlayer players[4];
};
```

The actual declarations also retain the named bounds and `NO_PLAYER`. Compile-time
assertions fix `sizeof(GolfPlayerScore) == 144`, `sizeof(GolfPlayer) == 206`, and
`sizeof(GolfRound) == 906`, and require standard-layout, trivially-copyable aggregates.
The RAM mechanism is deliberately simple: all four worst-case records are reserved once
inside the round, so enabling another player performs **no allocation** and cannot
fragment the C3 heap. The cost over the old 254-byte round is a fixed 652 bytes.

A 906-byte `GolfRound` must not be an automatic local on an embedded task stack. Keep it
in the persistent singleton or as a member of an already heap-allocated activity, and
pass references. Do not create temporary whole-round copies in rendering or mutation
paths.

`par`, `si`, and `hasSi` are shared. `CourseStore::applyGolfCourse()` snapshots all
three and keeps neutral player defaults; the setup-only selection helper then applies
§16.1. When `hasSi` is false, the shared `si` array stays zero. Each player's `yards`
array is selected from that player's tee and is independent of every other player. A
selection-only tee keeps that player's fixed yard array zero.

### 16.3 Explicit per-player APIs

Mutation cannot select a player implicitly. Rules and penalty calls take an explicit
`GolfPlayerScore&`; per-player statistics take both the shared `const GolfRound&` and an
explicit `const GolfPlayerScore&`. In particular, none of these APIs reads
`currentPlayer` to decide whose data to modify.

Representative calls are:

```cpp
incrementGolfCounter(player.score, hole, field);
seedGolfHoleAtPar(player.score, hole, round.par[hole]);
golfAppendPenalty(player.score, hole, field, kind);
golfHoleScore(round, player.score, hole);
golfScore(round, player.score);
```

Validation follows the same ownership boundary. `validateGolfPlayerScore()` repairs one
explicit score and returns that player's hole masks. `validateGolfRound()` validates all
four scores and retains four separate result records, so a repair can be logged with both
slot and hole. A persisted or playable round is rejected if no player is enabled or if a
tee token is invalid. An out-of-range or disabled `currentPlayer` is repaired to the
first enabled slot.

### 16.4 Flattened turn order and blank advance

The scoring order is the Cartesian sequence `(hole, enabled player)` in ascending stable
slot order. These allocation-free helpers are authoritative:

```cpp
uint8_t golfFirstEnabledPlayer(const GolfRound& round);
uint8_t golfNextEnabledPlayer(const GolfRound& round, uint8_t player);
uint8_t golfPreviousEnabledPlayer(const GolfRound& round, uint8_t player);
bool advanceGolfTurn(GolfRound& round);
```

The index helpers wrap and return `GolfRound::NO_PLAYER` when no slot is enabled.
`advanceGolfTurn()` moves to the next enabled slot on the same hole. From the last
enabled slot it moves to the first enabled slot on the next hole; the final hole wraps
to hole 0. With one enabled slot, every advance changes the hole exactly as the v3
single-player implementation did.

The positional `Out100` confirm rule still applies per turn. An unentered player/hole
with preview values commits that player's values before advancing. An unentered
player/hole with all-zero values advances without creating a score. No other player's
arrays are read or changed by either path.

### 16.5 State and completed-round JSON v4

Both codecs share one multiplayer payload. State files include `currentHole` and
`currentPlayer`; completed archives omit both cursor fields. `players` always has exactly
four ordered slot objects so disabled-player names survive:

```json
{
  "v": 4,
  "date": null,
  "course": "Sanyang Golf Club",
  "holes": 18,
  "currentHole": 6,
  "currentPlayer": 2,
  "par": [4,5,3,4,5,3,4,4,4,4,4,5,3,4,4,4,3,5],
  "hasSi": true,
  "si": [15,7,13,11,9,17,3,1,5,6,12,18,16,10,14,2,4,8],
  "players": [
    {"name":"Noah",     "tee":"Blue",    "yards":[0], "putts":[0], "in100":[0], "out100":[0], "penalties":[[]]},
    {"name":"Player B", "tee":"White",   "yards":[0], "putts":[0], "in100":[0], "out100":[0], "penalties":[[]]},
    {"name":"Player C", "tee":"NotPlay", "yards":[0], "putts":[0], "in100":[0], "out100":[0], "penalties":[[]]},
    {"name":"Player D", "tee":"NotPlay", "yards":[0], "putts":[0], "in100":[0], "out100":[0], "penalties":[[]]}
  ]
}
```

The one-element arrays above abbreviate the shape only; on disk every per-hole array is
exactly `holes` elements and `penalties` has exactly `holes` subarrays. `par` and `si`
are also exactly `holes` elements. Disabled players use `NotPlay` and canonical zero
yards/scores/penalties. A writer may omit a disabled zero-only array only if its decoder
specifies and applies that same all-zero default; writing the full shape is preferred.

Legacy v2/v3 input migrates into slot 0. It receives the decoded canonical Blue or White
tee; an absent or malformed legacy label uses the defined Blue fallback. Its yards,
scores, and penalties move unchanged, slots 1–3 use their default names and remain
`NotPlay`, and absent SI becomes `hasSi = false` with a zero SI array. v1 remains
rejected because its `in100` meaning is incompatible. The `archivedAs` commit marker is
still inspected before version, metadata, or score parsing and before allocating a
staging round.

Transactional decode uses one checked `makeUniqueNoThrow<GolfRound>()` staging object.
This allocation is required because a 906-byte automatic local exceeds the embedded task
stack policy, while decoding directly into the live store would expose partial state on
failure. File loading follows the same one-allocation rule; archive uses one larger
checked scratch allocation containing the round and its reusable CSV row buffers. No
allocation occurs inside a player or hole loop. The streaming index migrator contains
two 255-byte line buffers, so firmware also creates each migrator with one checked
`makeUniqueNoThrow` allocation per pass instead of placing it on the task stack; rows
still stream one at a time.

### 16.6 Normalized history index v4

The exact header is:

```text
date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,file
```

A completed group round owns one JSON file but contributes one index row for each enabled
stable slot. `playerSlot` is decimal `0..3`; `playerName` is the round-time name snapshot
and uses the same RFC-4180 quoting as `course`. Every row for the group carries the same
`file`. Disabled slots produce no row. The score aggregates are computed from the
explicit player's `GolfPlayerScore`; there is no group total and no implicit use of
`currentPlayer`.

A v2 or v3 index is migrated through `index.csv.new` and `index.csv.bak` before a v4
append. Every legacy row becomes slot `0`, name `Noah`; existing aggregate and penalty
recording semantics are preserved. The staged file must parse as v4 and retain the
expected row count before it replaces the live index. An unknown header is never mixed
with v4 rows.

Archiving stages the old index plus all 1–4 enabled-player rows, verifies their total
count, shared filename, and distinct slot mask, and only then swaps the staged file into
place. Thus a partial player group is never visible in live History. Recovery never
promotes a lone `index.csv.new` when both the live index and backup are absent: a first
multi-player append can lose power at a complete-row boundary and still be syntactically
valid v4 while containing only part of the group. The lone stage is discarded; failure
to remove it fails recovery, and the still-open round can retry by constructing the whole
group again. When a backup exists it remains the pre-transaction authority and is
restored before staging is touched; when a live index exists it remains authoritative.

Whole-round delete performs the inverse staged rewrite: it requires 1–4 distinct rows
for the filename, removes all of them, verifies none remain, and commits at the
`index.csv.new` to `index.csv` rename. Removing `index.csv.bak` is post-commit cleanup;
its failure does not suppress the JSON unlink or turn the already-published deletion
into a pre-commit failure. A retry that finds no matching rows treats the index side as
already committed and removes any remaining group JSON instead of stranding it. A JSON
unlink failure may still leave an invisible orphan but cannot leave a visible partial
group; later index recovery removes stale staging or backup artifacts.

History and Trends are always selected by stable slot. `GolfHistoryReader::reset(slot)`
rejects an invalid slot and filters matching rows **before** its 50-entry ring and
overflow count. `GolfIndexFileLocator::reset(slot, newestIndex, totalFilteredRows)`
applies the identical filter when resolving a row's shared filename.

`GolfPlayerNamesReader` scans chronologically with one fixed row buffer, starts with the
four default names, and keeps only the latest valid snapshot and presence bit for each
slot. The one shared selector owns this reader, a retained recovery migrator, and one
fixed I/O chunk. Each selector scan on entry and post-child rescan of an existing index
performs one necessary, checked `HalFile::Impl` handle allocation; allocation failure is
logged and publishes the rows as unavailable. There is no per-row allocation. A release/acquire
atomic refresh flag hides the previously published rows until the rescan publishes under
`RenderLock`, so whole-group deletion can disable the last affected selector row without
a stale activation window.

History and Trends constructors copy only the selected slot and its fallback name. Their
active/staging states stream and atomically pointer-publish only that slot; neither owns a
four-name snapshot or scans for a first-present slot. On the ESP32-C3 target the selector
object is 3,076 bytes. While it is retained beneath a child, the explicit object/scratch
payload is 17,100 bytes for History (3,076-byte selector + 7,488-byte activity +
6,536-byte checked scratch) or 15,156 bytes for Trends (3,076 + 6,756 + 5,324). These
figures exclude allocator metadata and the transient checked file-handle allocation and
are current accounting, not a net-saving claim. History preserves its selected-slot
second-pass locator and post-deletion reload. Trends folds only its already filtered
reader and keeps the rounds-count subtitle in content while the header identifies the
selected player.


## 16. Multiplayer UX pass (M1-M3)

Design with 1:1 mocks: `docs/golf/design/multiplayer-ux.html`.

### 16.1 The golf header is a fixed height (M1)

Golf screens currently size their header from `metrics.headerHeight`, which is the
**e-reader's book title bar** metric. It is not a constant: 45 px on Classic and
RoundedRaff, **84 px on Lyra**. The scorecard's chrome therefore changes size when the
owner changes reading themes.

A book header carries a title, progress and page counts and wants that height. A
scorecard header carries a player, a course and the time. Golf screens take their own
fixed **46 px** — the compact strip sized to the battery indicator plus its padding —
and stop reading `metrics.headerHeight` entirely.

The 38 px recovered on Lyra goes back to the scoring bands, which are the screens the
owner actually reads.

### 16.2 The totals band shows two cells, not three (M2)

The band splits into three equal cells — Thru, Score, To par — each a third of the panel
width. At the bold 12 pt face a two-digit score and a signed to-par collide.

Drop **Score**: to-par already contains it, since `score = par-so-far + to-par` whenever
par is known. Thru and to-par are also what a golfer says out loud.

**The second cell is positional, not fixed:**

| Course | Cells |
| --- | --- |
| Has par | **Thru · To par** |
| Par-free | **Thru · Score** |

The par-free case is not an afterthought. The Template course has no par, so dropping
Score outright would leave it showing Thru and a blank — losing the only running total it
has. Never both, never blank.

### 16.3 Player count is chosen once, on its own screen (M3)

After the course is chosen, ask how many are playing. The screen opens on **1** with
Confirm already the right answer, because most rounds are solo and the common case must
not be taxed to serve the rare foursome.

* Both the side rocker and the front Prev/Next step the count; people reach for whichever
  is under their thumb.
* Range is 1 to `GOLF_MAX_PLAYERS` (4). Show the ceiling as pips rather than a sentence.
* The Confirm hint reads **Start** at a count of 1, because it really is the last press.

**The roster screen is skipped entirely at one player.** One player means one name and
one tee, both already defaulted, so that screen has nothing to decide.

**Defaults.** Slot 1 is `Noah` on the blue tee. Slots 2-4 become **`Player 2`,
`Player 3`, `Player 4`**, replacing today's `Player B/C/D`: letters and numbers read as
two schemes when slot 1 is a name, and the numbers now match the row index beside them.
Slot 1 keeps a name because it is a person, not a slot. Every player is playable as-is,
so the roster is a review step, not a form.

**The count has exactly one owner.** There is no "add player" row on the roster; the
roster only reflects the count screen. Two places editing one value is how the home
screen's off-by-one bugs happened.


## 17. Scoring screen polish (v4.1)

Owner feedback after flashing the M1-M3 build. All three are presentation-only; no scoring
behaviour changes.

### 17.1 The header is not bold and not highlighted

The scoring header currently renders emphasised. It is chrome, not content: the player and
course name orient you, they are not the thing being read. Draw the header text at regular
weight, with no inverted or filled band behind it.

### 17.2 "Score zone", and no running total in that field

`STR_GOLF_ENTER_SCORING_ZONE` becomes **"SCORE ZONE"**. "Enter scoring zone" read as an
instruction — a button you press — when it is a label for a count.

The third counter row also draws a right-hand badge reading `TOTAL 4 E` (via
`STR_GOLF_TOTAL_TO_PAR_FORMAT`). **Remove it.** The totals band directly beneath already
carries Thru and To par, so the badge repeated a number the eye finds two rows lower, and
it competed with the counter value that row exists to show.

### 17.3 Counter digits follow the main-branch treatment

`main` draws each counter value at a fixed size with a downward nudge:

```
const int digitHeight = focused ? 100 : 66;
golfDrawLargeNumber(renderer, screenWidth / 2, top + (height - digitHeight) / 2 + 12, ...)
```

The multiplayer branch kept the same preferred 100/66 but clamped it to `rect.height - 10`
and dropped the `+ 12`, so on a short band the digit silently shrinks and sits higher. The
owner reads that as the number being too small.

Restore the main-branch look: the full 100/66 and the downward nudge whenever the band has
room. Keep a clamp only as an overflow guard for genuinely short bands — the clamp was not
wrong, it was just doing its work far more often than intended. §16.1 gave the counters
38 px back on Lyra, so there is now room the branch's clamp was written before.


## 18. The totals band carries THRU, HOLE and TOTAL (v4.2)

Supersedes §16.2. The two-cell band was right to drop the collision, but it dropped the
wrong things: the owner lost per-hole feedback when the `TOTAL 4 E` badge was removed from
the scoring-zone row in §17.2, and the band never showed a gross score.

Three cells:

| Cell | Shows | Scope |
| --- | --- | --- |
| **THRU** | holes completed | round |
| **HOLE** | to-par on the current hole, e.g. `+1`, `+2`, `E` | **this hole** |
| **TOTAL** | strokes shot so far | round |

`HOLE` is the one that moves while you are scoring: editing any counter on the current
hole changes it immediately, and it is the feedback the removed badge used to give —
now in the band where totals belong, instead of competing with the counter value in
its own row.

`TOTAL` is a stroke count, not a to-par. Sitting beside a signed `HOLE` value, an
unsigned total reads unambiguously as the gross score.

The round's cumulative to-par no longer appears on the scoring screen. It remains on
the scorecard, the round summary and trends, which are the screens for reviewing a
round rather than playing one.

### 18.1 Par-free courses

`HOLE` is a to-par and cannot exist without par. On a par-free course that cell shows
the **hole's stroke count** instead, with its label unchanged. Same positional
discipline as §16.2's second cell: never blank, never a fabricated `E`.

### 18.2 On the three-cell width

§16.2 removed a cell because the owner reported score and to-par colliding. That
crowding was at least partly the `TOTAL 4 E` badge drawn on the counter row directly
above, which §17.2 has since removed. Three cells are therefore expected to fit —
but measure the rendered widths at the bold 12 pt face before committing, and report
them. If they genuinely do not fit, stop and ask rather than silently dropping a cell.


## 19. HOLE reads the seeded score; the hole number gets breathing room (v4.3)

### 19.1 Bug: HOLE shows minus-par on an untouched hole

On an unentered hole the counters **display** seeded values — `2` putts, `2` inside 100,
`par - 2` in the scoring zone — which sum to par. But `drawTotals` reads
`golfHoleScore(round, score, hole)`, which returns the **stored** score, still `0`. So the
HOLE cell renders `0 - par`: `-3` on a par 3, `-4` on a par 4, `-5` on a par 5.

The screen contradicts itself — the counters show a par score while HOLE claims you are par
under it. The correct reading on an untouched hole is **`E`**.

**Root cause: two places compute the hole's score independently.** `drawCounters` derives
seeded values inline; `drawTotals` reads storage. §18 specified HOLE as "to-par on the
current hole" without saying which of those two it meant, and that gap is mine.

**Fix the class, not the instance.** Extract the seeded-or-stored decision into one shared
helper — the same discipline that `golfui::totalsSecondCell` and `golfCountConfirmLabel`
already follow — and have both the counters and the totals band read it. Any future band
that needs the current hole's score reads the same helper. Two independent derivations of
one value is the shape that produced this bug and the home-screen off-by-ones before it.

Seeded score is `in100 + out100` = `2 + (par - 2)` = `par`, so to-par is `0`. That falls out
of the helper rather than being special-cased.

The seeding precondition stays as it is (`par >= 3`): on a par-free course there is nothing
to seed, and §18.1 already governs that cell.

### 19.2 The hole number needs clearance

The hole number is bottom-aligned in its band at `rect.y + rect.height - digitHeight - 4`,
leaving four pixels above the rule beneath it, and sized up to 58 px. It crowds the line.

Reduce the digit size and lift it: the number should read as sitting *in* its band rather
than resting on the rule. Presentation only — the band's own height does not change, and
nothing below it moves.

### 19.3 Label rename: "SCORE ZONE" becomes "TO SCORE ZONE"

`STR_GOLF_SCORING_ZONE` becomes **"TO SCORE ZONE"**. Measured against the built-in
`ubuntu_10_bold` face, the new label is 162 px against 129 px for the old one. On the
scoring-zone counter row, focused, in portrait (the narrowest orientation), the space
between the label's left edge and the big digit's left edge is 193 px in normal play
(a single-digit `out100`) but only 160 px in the rare case `out100` reaches double
digits — 2 px short of the new label.

The fix recovers clearance from the row's own left padding rather than from the label
or the digit: all three counter rows share one `padding = minValue(20, rect.width / 8)`
at `GolfScoringActivity.cpp`, reduced to `minValue(16, rect.width / 8)`. That buys 4 px
of clearance against the 2 px deficit, worst case, without making the label reflow as
the counter crosses from one digit to two — a second place deriving a presentation
decision from the counter value would repeat the shape of the §19.1 bug.


## 20. Tees are applied where the count is set (v4.4)

### 20.1 The bug: a solo round cannot be archived

`validateGolfRound` fails a round with no enabled player (`tee != NotPlay`), so
`RoundArchive::archive` returns `FailedBeforeCommit` and the finish shows
*"Archive failed. The open round was preserved."*

Tees are only ever applied by `golfApplyPlayerCount`, called from two places:

* `stepPlayerCount()` — guarded by `if (next == playerCount) return;`, so it runs only
  when the count actually **changes**;
* `showPlayers()` — the roster phase, which §16.3 **skips at one player**.

`playerCount` starts at `1`, and the confirm handler routes a count of 1 to
`completeRound()`, which never applies tees. So accepting the default and pressing
Confirm leaves every tee at `NotPlay`. The round scores normally — nothing during play
reads a tee — and fails only at the finish.

This lands on the **default path**: the solo round §16.3 optimised to two presses.
Stepping to 2 and back to 1 masks it completely, because that fires `stepPlayerCount`,
which is why it survived review.

**The cause is a spec error in §16.3.** Telling the worker to skip the roster was right;
leaving the tee application inside the screen being skipped was not. Same shape as §19.1:
one value, two owners, and a path where neither runs.

### 20.2 Prevention: the count owns the tees

Apply the player count wherever `playerCount` is **established** — including its initial
value — not where the roster happens to render. After that, no route through setup can
reach `completeRound()` with unapplied tees, whether the roster is shown or skipped.

Do not fix this by adding a second `golfApplyPlayerCount` call inside `completeRound()`.
That leaves three call sites for one value and preserves the shape that caused the bug.

### 20.3 Recovery: an unarchivable round repairs to player 1

Rounds already on the owner's device carry zero enabled players and cannot be finished.
Refusing them permanently is the wrong outcome for data the owner actually played.

`validateGolfRound` **repairs** zero-enabled to player 1 enabled on the default tee, and
reports it like the existing `currentPlayerReset` repair, rather than failing the round.
The owner has authorised this explicitly: the stuck round is his, and it should archive
under `Noah`.

Repair, not silent acceptance: the round stays valid only because it was fixed, and the
repair is logged through `golfLogRoundRepairs` like every other.


## 21. An unreadable index is quarantined and rebuilt, never fatal (v4.5)

### 21.1 What went wrong on the owner's device

Finishing a round failed with *"Archive failed"*. Serial gave the exact cause:

```
[ERR] [GOLF] index recovery verify failed: /golf/rounds/index.csv (stream=0 version=0)
[ERR] [GOLF] index recovery failed: live=... staged=... backup=...
```

`stream=0` is `Complete` — the file read fine. `version=0` is `Unknown` — no header
matched. The owner's real file is kept at
`docs/golf/examples/index-legacy-mixed-schema.csv`:

| Line | Columns | Shape |
| --- | --- | --- |
| 1 (header) | 8 | pre-V2, no `out100` |
| 2 | 8 | matches the header |
| 3-6 | 9 | V2 |
| 7 | 11 | V3 |

**Three row schemas under a header describing none of them.** The header was written by
an early build and never migrated, while later builds appended rows in their own current
format. §5.3's append-only rule is what allowed the drift: appending never rewrites the
header, so once the row format moved on, the header could only fall further behind.

The row data itself is sound — line 4 reads `strokes=75, in100=37, out100=38` and
`37+38=75`; line 7 gives `36+36=72`. Only the header is stale.

### 21.2 A present-but-invalid index must never be fatal

Recovery today treats a missing index as `NoIndex` (fine, create a fresh one) but a
present-and-unrecognisable one as `Failed`. So a corrupt index is *worse than no index*,
and it bricks archiving permanently with no route out from the device.

An index that cannot be recognised is **renamed aside** — to a non-colliding
`index.csv.unreadable` — and a fresh one started. Saving a round must never become
impossible because a derived file is malformed.

### 21.3 Rebuild the index from the round files

`index.csv` is **derived**; `/golf/rounds/*.json` are the source of truth. After
quarantine, rebuild the index by scanning the round files, which the decoder already
reads at v2, v3 and v4.

Do not attempt to parse the quarantined file. No single header describes lines 2, 3-6
and 7, so a header-keyed parser mis-reads most rows; the round files are both simpler
and correct. The owner's six rounds — `round-0001` through `round-0005` plus
`2026-01-01-quick-round-all-par-4.json` — must all come back.

Rebuild is also the honest general answer: any index that disagrees with the round files
should lose, because the rounds are the data and the index is a cache.

### 21.4 Migrate the header when the format changes

The drift is only possible because appending never revisits the header. Whenever the
live index's version is recognised but older than current, the append path migrates it —
staged, verified, renamed, as §12.7 already does — rather than appending a new-format row
under an old-format header.


## 22. Navigation pass (v5)

Design with 1:1 mocks: `docs/golf/design/navigation-ux.html`. Owner-approved.

### 22.1 The main menu is three horizontal tiles

New round / History / Trends is a fixed set of three that never grows. A vertical list
is built for an unknown number of items; with three it wastes the screen and puts the
third destination two scrolls away.

Three tiles across the top, moved between with Left/Right, so **every destination is
visible and one Confirm away**.

Beneath the tiles sits a **detail panel describing the focused tile** — the last round's
course and score under New round, the recorded round count under Trends. Going horizontal
frees most of the panel; the detail is what that space buys, and without it the change is
only a rearrangement.

The Confirm hint follows the tile: **Start** for New round, **Open** for the two review
screens, so the button names its own effect.

### 22.2 Course rows carry par, holes and tees

The vertical list stays — the owner can see every course at once, which is the point.
What it lacked was content: a course was a bare name.

Each row now shows the name, its hole count and available tees, and **par as a figure on
the right**. Four courses still fit without scrolling. A par-free course shows an em dash,
never a zero, matching §18.1 and the card.

### 22.3 Back returns; Confirm starts

On the roster, Back currently tees off. It is the only Back in the app that goes forwards,
and that — not its label — is why it reads as confusing.

| Button | Action |
| --- | --- |
| **Back** | returns to the player count |
| **Confirm** | starts the round, hinted as **Start** |

Renaming Back to *Complete* was considered and rejected: it leaves one button doing two
unrelated jobs, and a button called Complete that moves backwards is its own puzzle.
§16.3 already made the roster a review step where every player is playable, so there is
nothing to complete — only "go back" and "start".

### 22.4 Player pickers list only players who have rounds

History and Trends share a picker that draws all four slots, disabling absent ones with
*No rounds*. On a single-user device three of four rows are permanent dead ends.

`publishPlayers()` already computes `presentMask` from the index and then draws every row
anyway. **Emit rows only for present slots**, and give each a round count, which is what
the owner would want to know before opening it.

**The empty case needs a real state.** If no player has rounds, filtering leaves nothing
to show, and a blank screen is not an answer. It must also stay distinguishable from the
existing load-failure message: *"no rounds yet"* and *"couldn't read your rounds"* call
for different reactions.


## 23. Header overlap, Power as Confirm, and a smaller detail box (v5.1)

### 23.1 Bug: the right-hand header label overlaps the battery

`golfui::drawHeader` draws the battery at `rect.y` — the top of the band — while the
manual right-label path draws at `rect.bottom() - borderWidth - spaceSm - labelHeight`,
the bottom. The two are **stacked vertically**, which the upstream book header had room
for. §16.1 fixed the golf header at 46 px, and the stack no longer fits.

Every screen passing a right label is affected, not only the two the owner noticed:

| Screen | Right label |
| --- | --- |
| Scorecard | course name |
| History | player label |
| History round menu | round status |
| Hole review | round status |
| Trends | player label |
| Scoring | clock, once the clock is set |

Home and Message pass `nullptr` and are unaffected.

**The label sits beside the battery, not below it.** Reserve width for both in a single
row, and shorten the label rather than let it collide. The 46 px header is correct and
does not change; the two-row assumption inside `drawHeader` is what was wrong.

### 23.2 Short-press Power confirms — except while scoring

`PWR_CONFIRM` already exists as a `SHORT_PWRBTN` setting, and `wasPressed`/`wasReleased`
already fold `wasPowerConfirmClick()` into `Button::Confirm`. It is unreachable on the X4
only because `wasPowerConfirmClick()` sits behind `#if FREEINK_CAP_TOUCH` and additionally
tests `gpio.hasTouch()`. The X4 has no touch, so the feature compiles out.

Make it available on non-touch boards.

**The scoring screen keeps Power for field cycling.** §12.6 gives Power the field-cycling
job so scoring is one-handed, and the owner has confirmed he wants that kept. Without care
a single Power press would both cycle the field and open the penalty picker, because
`powerCyclesField()` returns true on X4 while `Button::Confirm` would also fire.

Scoring must therefore be able to tell a Confirm that came from Power from one that came
from the front button, and act only on the front button. Every other screen treats them
identically. Solve it once, in the input layer, rather than by having each activity guess.

### 23.3 The side rocker also moves the main menu

The main menu tiles move with Left/Right. Up/Down on the side rocker moves them too — on
this hardware people reach for whichever is under the thumb, the same reasoning §16.3
applied to the player count. Left/Right keeps working.

### 23.4 The detail box shrinks to its content

The panel under the tiles was sized to fill the space going horizontal freed, before it
was known how little text lands in it. It should be as tall as its content needs and no
taller. The tiles keep their size; the space returns to the panel background rather than
to an empty box.
