# On-device test plan — first flashable build

Everything below the UI has host tests (48 of them). Nothing that touches the SD card,
the panel, the buttons, or the sleep cycle does. **This plan is ordered by how much a
failure would cost, not by how the app is used.**

## Why device testing carries unusual weight here

These layers are verified by reading only. No test suite can reach them:

| Untested layer | Why it cannot be host-tested | What a bug costs |
| --- | --- | --- |
| `RoundArchive` collision loop | Needs `HalStorage` file existence checks | **A completed round silently overwritten** |
| The four persistence triggers | Needs SD + the real sleep cycle | A round lost in your pocket |
| `main.cpp` resume-on-wake | Needs a real deep-sleep wake | Wake lands on the reader, not your hole |
| All `HalStorage` paths | Needs a real card | Nothing persists at all |
| Panel legibility and refresh | Needs eyes and sunlight | Unusable on a cart |

## Tier 1 — data integrity. Do these first, indoors.

1. **Course loads.** Copy `docs/golf/examples/pebble-beach.json` to
   `/golf/courses/pebble-beach.json`. It should appear in the picker as *Pebble Beach*.
2. **A bad course file is skipped, not fatal.** Copy `sanyang-suzhou.json` alongside it.
   Expect Pebble Beach still listed, Sanyang absent, and a log line naming the file and
   `par must be 3 through 6`. This is a real test of the §5.1 validation path using a
   genuinely invalid fixture, and it proves one bad file cannot hide the others.
3. **Score a hole, sleep, wake.** Enter a few strokes, sleep via the power button, wake.
   You must land **on the scoring screen at the same hole**, not on home or the reader.
   This exercises the `main.cpp` branch and trigger 3.
4. **Auto-sleep survives too.** Repeat, but let the idle timeout sleep it rather than
   pressing power. Both paths route through `enterDeepSleep()`; confirm both.
5. **Hard power loss.** Enter a hole, then hold power off / pull power without a clean
   sleep. On reboot you should lose at most the current hole's counters — that is the
   accepted worst case in §6, not a bug.
6. **Finish a round.** Confirm `/golf/rounds/<date>-pebble-beach.json` and a row in
   `index.csv` (pull the card, or use the webserver's `/files` page).
7. **THE COLLISION TEST — the single most valuable thing you can do.** Finish a *second*
   round, same course, same date. Expect `...-pebble-beach-2.json`, **both files
   present and distinct**, and two correct rows in `index.csv`. This loop has never
   executed anywhere. If it is wrong, it destroys rounds.

## Tier 2 — the interaction model

8. **Focus legibility.** Can you tell at arm's length, in bright light, which counter
   `Up` will move? The focused row should be inverted *and* taller. If you have to hunt
   for it, the design is wrong and I want to know.
9. **Presses per hole.** A par-4 with 5 strokes, 2 putts, 1 wedge should cost **7
   presses** with pre-seed to par on. Count it. Materially more means the model needs
   rework.
10. **The invariant, felt rather than asserted.** With strokes 5, putts 2, in100 3, press
    `Down` on strokes: it must refuse and flash **both** putts and in100. Then press
    `Down` on a counter already at 0: it must do nothing and flash **nothing**. Those
    two behaving differently is the whole point of §2.1.
11. **Auto-bump.** With strokes equal to putts+in100, press `Up` on putts — strokes
    should follow.
12. **Unentered holes stay blank.** Walk past a hole without pressing anything, then
    check the card. It must be blank, never a silent par.
13. **Ghosting.** After ~20 presses, is the panel still clean? The ghost-clear promotes
    every 8th paint.

## Tier 3 — the ugly paths

14. Remove the SD card and open the golf app → readable message, no hang, no crash.
15. Empty `/golf/courses/` → readable message, and *Quick round* still offered.
16. Corrupt `state.json` by hand (break the JSON) → rejected with a log line, app usable.
17. Delete `/golf` entirely → recreated on demand.
18. **Both destructive confirmations.** Abandon and Finish must each prompt. Try
    cancelling each; nothing should change.

## Tier 4 — not built yet, do not test

* **History** — stub this milestone.
* **Round detail, stats, the three-bucket breakdown** — M3.
* **CSV export** — M3. Rounds still come off as JSON over the webserver.
* **On-device course editor** — M4. Author course files on a computer.

## A sensible first session

Sit at home with the device and play nine imaginary holes of Pebble Beach: score them,
sleep and wake at least twice mid-round, finish, then do it again to trigger the
collision path. That covers Tier 1 end to end in about ten minutes with nothing real at
stake — and it is a far better first outing than discovering a persistence bug on the
7th at Sanyang with a card you cared about.

## What to report back

The refresh feel, the focus legibility, the press count, and **anything that hung**.
A hang matters more than a wrong number: numbers I can fix from a log, but a hang on a
cart means the device is dead weight for the rest of the round.
