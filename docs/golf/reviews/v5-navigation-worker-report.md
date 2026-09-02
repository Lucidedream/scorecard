# v5 Navigation Worker Report

## Outcome

Implemented the approved v5 navigation pass without adding an upstream touchpoint or creating a task:

- The golf home screen presents its fixed destinations as a horizontal tile row, supports two or three tiles according to `showNewRound`, and describes the focused destination below the row. New Round uses a Start hint; History and Trends use Open.
- Course rows retain the vertical list while adding hole count, available tees, and right-aligned par. Par-free courses show an em dash.
- The roster is a review step: Back returns to player count and Confirm starts. The count-one Confirm path still bypasses the roster, initializes hole index zero, persists the round, and opens scoring.
- The shared History/Trends player picker emits only slots that have indexed rounds, shows each slot's round count, maps compact rows back to stable player slots, and distinguishes an empty index from a load failure.
- All button hints continue through `mappedInput.mapLabels()` and `GUI.drawButtonHints()`.

No new heap allocation or `xTaskCreate` call was introduced. Home and picker summaries use fixed-size members; course display metadata adds bounded storage to the activity object, avoiding repeated allocation and fragmentation while rows are rendered. Index reads complete before rendering, so no `RenderLock` spans SD I/O.

## Measured vertical fit at 800 px

The calculation uses the repository's 480 x 800 X4 test geometry (top/right/bottom/left insets 7/9/11/13), the 40 px footer reserve, the actual theme top padding and spacing, and the 46 px golf chrome header.

- Course row height is 96 px. RoundedRaff fits six complete rows and Lyra fits seven; therefore all four courses fit without scrolling in either theme.
- The home tile row is 120 px. The focused detail panel receives 552 px in RoundedRaff and 550 px in Lyra when no resumable round is shown; with the existing 64 px Resume control it receives 478 px and 470 px respectively. Every destination remains visible and the screen requires no vertical navigation.
- When `showNewRound` is false, the same available width is divided evenly between History and Trends; no placeholder or empty third tile is emitted.

## Verification

- Golf host tests: **229/229 passed** (the prior 225 plus picker coverage for partial presence, no rounds, load failure, and compact-slot mapping).
- Golf build: **succeeded**. `firmware.bin` is **5,547,040 bytes**, a **+3,392-byte** delta from 5,543,648. PlatformIO reports **84.4% flash** (5,533,329 / 6,553,600) and **17.4% RAM** (57,140 bytes).
- Default build: **succeeded**. The symbol audit found only `ScorecardIcon` and no other golf symbols.
- Formatting: `PATH=/opt/homebrew/opt/llvm/bin:$PATH ./bin/clang-format-fix -g` leaves no formatting changes.
- Patch integrity: `git diff --check` passes.
- Upstream touchpoints: **15**, unchanged.
- Solo path: code and host tests confirm count 1 -> Confirm -> persisted round at hole index 0 -> scoring (Hole 1), without showing the roster.

The remaining owner check is on-device visual and button confirmation, especially the solo path and the four-course fit on the physical panel. Heap can be monitored through the existing serial diagnostics before entering golf, on the home/detail screens, and after leaving golf to confirm the bounded activity storage is released with the activity lifecycle.
