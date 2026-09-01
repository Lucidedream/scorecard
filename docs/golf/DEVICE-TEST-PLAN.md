# On-device test plan — fixed-roster multiplayer

The fixed-memory multiplayer domain, turn traversal, rules, penalties, validation, and
statistics have host tests. Nothing that touches the SD card, panel, buttons, setup flow,
or sleep cycle does. **This plan is ordered by how much a failure would cost, not by how
the app is used.**

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
3. **Course-selection feedback and roster gate.** Select Sanyang: setup must open with
   P1 Noah already enabled on Blue, P2–P4 at *Not play*, and Complete immediately
   available. Repeat with an exact White-only SD course: P1 must default White. Repeat
   with `tees: ""` and no `yards`: both Blue and White must be selectable with zero
   displayed yardages and P1 must default Blue. Finally try `tees: "Blue/White"`, then
   an empty tee label with a nonzero yard array: neither may guess a tee, all four slots
   stay disabled, and Complete stays disabled. Enable slot 0 and slot 2 manually on a
   valid course and verify the stable disabled slot 1 is never compacted or renamed.
4. **Score a turn, sleep, wake.** Enter a few strokes for slot 2, sleep via the power
   button, and wake. You must land **on the scoring screen at the same hole and player**,
   not on home, slot 0, or the reader. This exercises the v4 cursor and trigger 3.
5. **Auto-sleep survives too.** Repeat, but let the idle timeout sleep it rather than
   pressing power. Both paths route through `enterDeepSleep()`; confirm both.
6. **Hard power loss.** Enter a turn, then hold power off / pull power without a clean
   sleep. On reboot you should lose at most the current player/hole counters — that is
   the accepted idle-flush window, not a reason for another slot to change.
7. **Finish a round.** Confirm `/golf/rounds/round-NNNN-pebble-beach.json` and exactly
   one `index.csv` row per enabled slot. With slots 0 and 2 enabled that means two rows,
   carrying slot/name snapshots and the same group filename. Open the JSON and verify
   exactly four ordered player objects, shared par/SI, and no handicap/net field.
8. **Player selector, group deletion, and collision.** History and Trends must each open
   a four-row P1–P4 selector; absent slots stay visible, disabled, and say the translated
   short *No rounds*. Front Next/Previous skips those rows, touch and Confirm cannot open
   one, and an all-absent selector still returns with Back. Open either present player's
   History, delete the group, and return: both index rows and the one JSON must disappear,
   the selector must rescan before showing old rows, and any slot whose last round was
   deleted must now be disabled. Then finish two more rounds and confirm both sequence
   files remain distinct; the safety suffix must still prevent overwrite if a sequence
   filename already exists.

## Tier 2 — the interaction model

9. **Focus and player legibility.** At arm's length in bright light, confirm both the
   focused field and current player are unambiguous. A counter press must never require
   guessing whose score will change.
10. **Sparse-slot turn order.** With slots 0 and 2 enabled, advance from slot 0 to slot 2
    on the same hole, then from slot 2 to slot 0 on the next hole. Slot 1 must never
    appear. At hole 18 the latter transition wraps to hole 1.
11. **One-player compatibility.** Disable slots 1–3. Leaving `Out100` must advance one
    hole exactly as before multiplayer; it must not spend an extra turn on the same hole.
12. **Score isolation.** Give two players visibly different values, then add and remove
    counters and penalties for each. Revisit both turns and verify neither operation
    changed the other player's score or markers.
13. **Unentered holes stay blank.** Advance from a zero-valued player/hole without
    touching a counter. That player/hole stays blank on the card while the next enabled
    turn is selected. A pre-seeded par commits only when values actually exist.
14. **Different tees.** Use Blue for one player and White for another on a course with
    both yardage sets. The shared par/SI must match while the displayed yardages differ.
15. **Names, selected identity, and translations.** Rename one slot, archive a round,
    and verify both selectors show the stable `Pn name` row. History and Trends must show
    only that slot's data with the same identity at the header right; neither screen may
    show tabs or switch players on Left/Right. Trends keeps *Average over N rounds* in
    the content. Switch locale and verify the name is preserved while Player/Not
    play/Blue/White/No rounds labels translate normally.
16. **Ghosting.** After ~20 presses and several player transitions, the panel should
    remain clean; the ghost-clear still promotes every 8th paint.

## Tier 3 — the ugly paths

17. Remove the SD card and open the scorecard → readable message, no hang, no crash.
18. Empty `/golf/courses/` → readable message, no hang, no fabricated course data.
19. Corrupt `state.json` by hand (break the JSON) → rejected with a log line, app usable.
20. Delete `/golf` entirely → recreated on demand.
21. **Both destructive confirmations.** Abandon and Finish must each prompt. Try
    cancelling each; nothing should change.
22. **First-group staging power cut.** With no `index.csv` or `index.csv.bak`, interrupt
    a two-player first archive after one complete row has reached `index.csv.new`. On
    restart, recovery must delete the lone stage rather than publish its syntactically
    valid partial group; retrying Finish must recreate both rows from the still-open
    round. Make the stage read-only once as a fault injection: recovery must fail and
    leave it unpublished until cleanup is possible.
23. **Delete publication cleanup cut.** Interrupt or fault-inject backup removal after a
    whole-group delete has renamed `index.csv.new` to `index.csv`. The JSON must still be
    unlinked, History must show none of the deleted rows, and the next recovery must
    remove `index.csv.bak`. Also cut power immediately after the index rename but before
    JSON unlink; retrying the delete with no matching rows must remove the remaining JSON
    rather than report a permanent failure.

## A sensible first session

Sit at home with the device and play nine imaginary holes with slots 0 and 2 enabled on
different tees: alternate turns, intentionally leave one player/hole blank, sleep and
wake at least twice, then finish. Repeat once to exercise sequence/collision handling.
That covers roster stability, score isolation, v4 persistence, and resume without putting
a real card at risk.

## What to report back

The refresh feel, the focus legibility, the press count, and **anything that hung**.
A hang matters more than a wrong number: numbers I can fix from a log, but a hang on a
cart means the device is dead weight for the rest of the round.
