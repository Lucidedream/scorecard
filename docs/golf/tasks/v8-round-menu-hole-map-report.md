# Round menu Hole map (§42)

The round menu now has View card, Hole map, Abandon round, Finish round, Send and Tips.
The Hole map label uses `tr(STR_GOLF_HOLE_MAP)` with the English fallback.

`src/activities/golf/GolfRoundMenuRows.h` defines named row identities and bounds-checks
row indices. Both labels and dispatch use those identities; invalid rows are ignored
rather than falling through to Abandon. Two host tests cover all six row identities,
the second-row map placement, and out-of-range inputs.

`src/activities/golf/GolfNavigation.cpp` owns the shared `openGolfCourseMap` helper. It
reads the live round by const reference and passes currentHole, holeCount, courseName
and `golfCourseHoleNumber(round, round.currentHole)` to the existing map activity.
It pushes that activity through the caller's `startActivityForResult`, so the menu stays
on the activity stack and Back returns to it. Back from the menu resumes the retained
scoring activity; neither route changes the hole or player. The helper's only allocation
is the existing checked `makeUniqueNoThrow<GolfCourseMapActivity>` moved from scoring:
the map must outlive this function, so a stack-local activity cannot serve its lifecycle.
No additional image buffers or growing collections were added, and locals remain below
256 bytes.

`src/activities/golf/GolfScoringActivity.cpp:582-586` keeps the existing flush before
opening a map, then calls the same helper. MENU long-press detection, timing and event
ordering are unchanged. Opening the menu already flushes at `GolfScoringActivity.cpp:558`.
The menu map action and helper never mutate the round or save it. The existing map
activity still handles missing maps and explicit catalog mapsDir lookup.

## Owner follow-up: remove text credit

Per the coordinator's owner instruction, removed the picker credit text and its entire
`screen.takeBottom` reservation from `GolfCoursePickerActivity.cpp:203` onward, so the
Regions list receives the full content area without a leftover credit gap. Removed the
Course info credit fact from `GolfCourseInfoActivity.cpp` and deleted
`STR_GOLF_DATA_BAIGOLF` from English YAML; no other translation contained it. A source
and translations search finds no remaining references or text credit. Exported BMPs,
image watermarks and exporter code are untouched. The coordinator's §41.1 and §41.5
contract amendments are included alongside §42.

## Verification

Source review verifies one map-constructor call shared by both entry points, child
navigation through `startActivityForResult`, unchanged long-press handling and flush,
and no round mutation in the new map route. Existing source-format and host checks are
recorded below with the final firmware results.

On device, short-press MENU, select the second row, view the current hole map, Back to
the same menu row, and Back again to scoring on the same hole/player. Compare with the
existing MENU long-press route, including a non-default nine selection and a missing
map. Confirm Abandon/Finish still open their matching confirmations, and check buttons,
touch and all orientations. Open Regions and Course info to verify the removed credit
and full-height list; any watermark inside a map image should remain visible.

No push or flash was performed; runtime navigation and appearance remain device checks.

- Final host build passed without warnings.
- Full CTest with the required macOS `CPLUS_INCLUDE_PATH`: 558 entries, 557 passed,
  one expected environment-gated catalog check skipped. Includes both new menu tests.
- Repository `./bin/clang-format-fix -g` and `git diff --check` passed.
- Logs: `/tmp/section42-host-build.log`, `/tmp/section42-ctest.log`,
  `/tmp/section42-x4pro.log`, `/tmp/section42-golf.log`.

Both firmware targets use the installed `~/.platformio` core without a
`PLATFORMIO_CORE_DIR` override; their build outputs are retained.

| Environment | RAM | Flash | Result |
| --- | --- | --- | --- |
| golf-x4pro | 90,700 / 327,680 B (27.7%) | 5,543,634 / 6,553,600 B (84.6%) | passed |
| golf | 57,276 / 327,680 B (17.5%) | 5,655,617 / 6,553,600 B (86.3%) | passed |

Both final firmware builds passed without warnings. No additional build target was run.
