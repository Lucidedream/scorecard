# P3 catalog Play implementation

New round now opens the Play picker, and Course info offers Play this course under the
same home-entry predicate as New round. No push or flash was performed.

## Implementation and evidence

- `src/activities/golf/GolfHomeActivity.cpp` routes New round through `openGolfNewRound`; navigation resets
  its RAM context and creates the Play picker (`src/activities/golf/GolfNavigation.cpp:83-111`). Hub Recent
  and region selections load the explicit course file into the picker's existing scratch
  course, then call the shared start helper (`src/activities/golf/GolfCoursePickerActivity.cpp:230-259`).
  Two or more nines open the nine picker; one nine opens player setup, using
  `golfDefaultNineSelection` in both cases.
- My courses in Play replaces the picker with the existing `GolfSetupActivity`. Its
  enumeration, bounded summaries, ordering, details and overflow row are unchanged.
  Selection uses the same shared start helper (`src/activities/golf/GolfSetupActivity.cpp:87-97`).
- Navigation retains a bounded `GolfSetupReturn` value, with a compile-time size limit
  below 128 bytes. It stores mode, level, region, sort, selected row and the My courses
  parent/fallback state. Course info returns also store the explicit file and action row.
  Nothing writes this navigation state to SD. Reconstructing a picker reloads its bounded
  region records and cached sort keys, then restores selection. If the catalog vanished
  or its saved region no longer exists, return falls back to the hub.
- Course info has one additional row and an English YAML key; other languages use the
  existing fallback. Its visibility and activation both use the shared home-entry decision
  (`src/activities/golf/GolfCourseInfoActivity.cpp:106-108,156-159`). Home's existing load/cleanup logic moved
  into `golfReadHomeEntry`, preserving archive-marker precedence. Back reconstructs Course
  info at the Play row, with its Browse parent, including My courses, available afterward.
- The coordinator confirmed the literal predicate and corrected §41.5: Play behaves
  exactly like New round, including today's replace-without-prompt behavior when a round
  is already open. No additional discard guard or confirmation was invented.

## Existing persistence and lookup paths

No data format or course-lookup order changed. These paths already support catalog
courses; no `/golf/courses` assumption needed a downstream fix:

- Creation: `src/golf/CourseOrder.cpp:116` copies the course name into the round;
  `src/activities/golf/GolfPlayerSetupActivity.cpp:370-406` composes and saves the round through the existing
  round store. The synthetic catalog test composes a round and resolves its stored name.
- Reboot/resume: `src/golf/GolfRoundStore.cpp:89` writes `course`, and `src/golf/GolfJson.h:258` decodes it.
  `resumeGolfRound` loads the same state and opens scoring. Round codec tests pass in the
  full suite; an actual reboot remains a device check.
- History: `src/golf/RoundArchive.cpp:695` streams the course name into the archived JSON; the
  index writer copies it into CSV (`src/golf/GolfCsv.cpp:343`), and
  `src/golf/GolfHistory.cpp:64` retains the parsed course name. History/index/archive-related host tests pass in the full suite.
- Recent: `src/activities/golf/GolfCoursePickerActivity.cpp` retains P2's backward CSV scan and resolves names
  with `CourseStore::findByName`; the new round uses the same stored name. Existing Recent
  tests and the new synthetic catalog-name lookup pass.
- Map glance: `src/activities/golf/GolfCourseMapActivity.cpp:34-36` resolves the name through `findByName` and
  uses `golfCourseMapsDir`. `src/golf/GolfCourseMaps.cpp:7-9` prefers the explicit `mapsDir`.
  The synthetic test verifies a composed catalog-course round resolves to `bg-321.json`
  and selects `bg-321`, rather than the course-name slug. `src/golf/CourseStore.cpp:233-265` keeps
  owner overrides, then built-ins, then catalog lookup.

## Memory and verification limits

The picker reuses P2's one page of names, bounded record arrays and checked scratch
allocation. Selecting a course in Play replaces the picker, releasing its scratch and
SD handle through the existing lifecycle. My courses reuses its existing checked
allocations; no new vector, growing name cache or per-row allocation was introduced.
The checked picker activity allocation is necessary because activities outlive the
navigation function and ActivityManager owns them. Course info and map-list recreation
use the same checked activity allocation pattern; stack locals remain below 256 bytes.
The additional Course info action uses bounded activity member storage.

Host/build results cannot verify touch, buttons, E-Ink appearance, actual reboot or runtime
heap. On device, use a synthetic SD catalog and the real export separately: start from
Recent, region Rating/Name and My courses; Back from nine/player setup to the exact row;
start from Course info and return to its Play row, then back through its Browse parents.
Start a catalog round, enter a score, sleep/reboot and Resume, open its map glance, finish
it, and check History plus Recent. Repeat with catalog absent and after removing a saved
region. Check all orientations on X4 buttons and X4 Pro touch, and monitor existing
`LOG_DBG` entry heap logs for stable free heap above 50 KB through repeated navigation.

## Automated verification

- Full CTest with the required macOS `CPLUS_INCLUDE_PATH`: 556 entries, 555 passed,
  environment-gated `golf_catalog_check` skipped. Four new synthetic tests cover return
  context, Browse/My courses info returns and fallback, every home-decision input
  combination, and catalog-round composition/name/map-directory resolution.
- Separate `GOLF_CATALOG_DIR=/tmp/golf-catalog-full ctest -R golf_catalog_check`: passed,
  reading all 425 exported JSON courses in place. No export data was copied into Git.
- Host build: passed with no warnings. Repository formatting wrapper and
  `git diff --check`: passed.
- All required firmware targets passed after the final code change:

| Environment | RAM | Flash | Result |
| --- | --- | --- | --- |
| golf-x4pro | 90,700 / 327,680 B (27.7%) | 5,543,698 / 6,553,600 B (84.6%) | passed |
| golf | 57,276 / 327,680 B (17.5%) | 5,655,147 / 6,553,600 B (86.3%) | passed |
| default | 56,164 / 327,680 B (17.1%) | 5,445,351 / 6,553,600 B (83.1%) | passed |

The `.pio/build/golf-x4pro` and `.pio/build/golf` outputs are retained;
`.pio/build/default` was deleted after passing. Logs are `/tmp/p3-host-build.log`,
`/tmp/p3-ctest.log`, `/tmp/p3-catalog-check.log`, `/tmp/p3-x4pro.log`,
`/tmp/p3-golf.log`, and `/tmp/p3-default.log`.

Builds use the installed `~/.platformio` core with no `PLATFORMIO_CORE_DIR` override.
The SDK's initial GitHub fetch failed; submodule initialization succeeded using a local
clone of the same pinned SDK revision. JPEGDEC's download also failed; its matching
installed dependency folder was copied from `~/src/scorecard/.pio/libdeps/golf`, as
allowed in the task, and matching cached dependency folders supplied the later targets.
The pinned WebSockets dependency emits its existing `NetworkClient::flush()` deprecation
warning on first compilation; no project-source warnings were emitted and no dependency
sources were edited.
