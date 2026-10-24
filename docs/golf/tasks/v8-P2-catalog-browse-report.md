# P2 catalog browse implementation

Browse mode is implemented; New round still uses its existing setup flow. Nothing was
pushed or flashed.

## Behaviour and data

- Course Map opens the picker hub: Recent section with up to five resolved courses,
  My courses, and By region when both the catalog header and index length validate.
- Recent streams index.csv backwards with the existing CSV parser, across players,
  skipping malformed/incomplete rows, duplicates, and unresolved names. Successful
  candidates resolve once directly into retained file handles, stopping at five.
- With no catalog, My courses still lists built-ins and owner files. Resolvable Recent
  entries still appear; without Recent the hub is skipped and Back from My courses
  returns Home. By region is hidden for missing/bad catalog headers or index lengths.
- Region courses use continuous button/touch scrolling via ListProps item windows,
  materializing only the visible page. My courses reuses the existing bounded list,
  as confirmed by the coordinator. No explicit paging rows were added.
- Each region has at most 512 uint16_t record ordinals and one uint8_t rating key per
  record. Keys load once with records; Rating sorts only RAM keys, and Name restores
  source record order without sorting or SD reads. Unrated courses sort last.
- Course info shows available facts, six category bars when present, and only Hole maps
  and Tee info actions. Missing/malformed catalog facts leave the course usable.
- Explicit course file identities reach maps/tee info. Name lookups search owner files,
  then built-ins, then catalog files, matching the coordinator's amended §41.4.
- GolfCourse adds only mapsDir[16] (size 698 bytes); catalog facts live separately and
  load only for Course info. Both map entry points use mapsDir with slug fallback.

## Memory and logging

The picker allocates one checked scratch object on entry for its regions, large course,
record and streaming buffers, avoiding large stack locals or permanent global storage.
Recent uses a checked temporary reader with fixed buffers, released after loading. The
in-round map lookup uses a checked temporary course because the course exceeds the
stack-local budget. All activity allocations use makeUniqueNoThrow and log OOM.
Catalog name scanning is a separate helper, keeping its record and 96-byte buffer out
of the owner-scan stack frame, without adding another heap allocation. SD I/O uses HAL.

LOG_DBG reports ESP.getFreeHeap() at Hub, Regions, Region courses, My courses and Course
info entry. Recent also logs elapsed milliseconds and the resolved course count. Check
these on device through repeated navigation; runtime heap has not been measured here.

## Clarifications

The coordinator confirmed Recent refills with older resolvable names to five, existing
My courses behaviour stays intact, and scrolling should use the SDK's window support.
For absent catalog facts, Holes uses all course nines; Par uses the default first two
nines (or the only nine); Yards uses the longest yarded tee over those same nines.
Zero Par and unavailable Yards are omitted. Present catalog fields override derivations.
During review the coordinator amended §41.4 to preserve owner overrides of built-ins,
and §41.5 to require cached rating keys and record-order Name browsing; those amendments
are included with the implementation.

## Verification

- Full CTest: 552 entries, 551 passed and the environment-gated catalog check skipped.
  Includes 16 new synthetic tests. All use the requested CPLUS_INCLUDE_PATH.
- Separate GOLF_CATALOG_DIR real-export check: passed, validating five files. The
  export remained outside the repo and was never copied into it.
- Final focused catalog run: all 17 entries passed, including the real-export check.
- Host build completed without warnings after the final data-layer changes.
- Formatting: repository clang-format wrapper; git diff --check passed.
- All three firmware targets passed in the requested sequence.

| Environment | RAM | Flash | Result |
| --- | --- | --- | --- |
| golf-x4pro | 90,636 / 327,680 B (27.7%) | 5,540,982 / 6,553,600 B (84.5%) | passed |
| golf | 57,212 / 327,680 B (17.5%) | 5,652,213 / 6,553,600 B (86.2%) | passed |
| default | 56,164 / 327,680 B (17.1%) | 5,445,207 / 6,553,600 B (83.1%) | passed |

Builds use the installed PlatformIO core, with output, temporary files and build cache
in this external-drive worktree. The redundant local core copy is removed after builds.
The golf-x4pro and golf build directories are retained; default is removed after passing.
The pinned WebSockets dependency emits a NetworkClient::flush deprecation warning on
its first compilation; no dependency sources were changed.

Device verification remains: open Recent/My courses/By region with and without a
catalog, toggle sorts and scroll across page boundaries, inspect full and partial facts,
open maps and tee info, return through each Back level, and check all four orientations
with buttons (X4) and touch (X4 Pro). Monitor entry heap logs for leaks and >50 KB free
heap through repeated visits. No hardware, touch, E-Ink appearance or runtime heap claims
are made from host/build results.
