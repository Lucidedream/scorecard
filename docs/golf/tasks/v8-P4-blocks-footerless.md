# Task v8-P4: course picker in blocks, and no footer on the X4 Pro

Worktree: `/Users/noah/orca/workspaces/scorecard/course-catalog-p4` (Mac disk), branch
`Lucidedream/course-catalog-p4`, based on `Lucidedream/course-catalog-p3` (`ac928a0a`).
First: `git submodule update --init freeink-sdk`.

Spec: **CONTRACTS-V2 §43 and §44** (appended in this worktree, uncommitted). The mocks are
`docs/golf/design/catalog-blocks.html` (open it in a browser; 1:1 at 480×800). Background:
§22, §25.3–25.4, §34.3 (tile screens), §37 (X4 Pro input), §41.4–41.5 (the picker), §42.

## Build, in this order

1. **Shared tile component** (§43.1). Factor the main menu's tile strip and detail box
   out of `GolfHomeActivity`. Prove the main menu is unchanged on the X4 by rendering it
   before and after, and comparing the framebuffer or the layout rects in a host test.
   Also refactor the History chooser onto the same component, if it uses the same code.
2. **Icons** (§43.1): add `history`, `flag`, `map-pin` and `table` to
   `golfTileIcons.manifest`. Fetch only those Lucide SVGs, from the pinned Lucide release
   matching the existing icons where you can tell, e.g. via unpkg `lucide-static`.
   Regenerate the header and confirm the existing icons are byte-identical.
3. **Picker restyle** (§43.2–43.5): hub, region grid sorted by count, course cards with
   the sort segment, Course info tiles plus facts. Keep the §41.5 behaviour and the P3
   return context.
4. **Footer-less touch boards** (§44): implement it once, in the shared footer path
   (`golfui::drawFooter` / `FooterInput` / chrome layout), with Scoring opting back in.
   Then add the listed replacements: swipes for Hole review / Tip note / Round export,
   the tappable hole band, the Send and Open/Done blocks, and the Player setup last row.
   Paging swipes must ignore `wasEdgeSwipe(Left)`.

## Constraints

CLAUDE.md throughout. All text via `tr()`. C3 RAM bounds as in §41.4. The X4 (no touch)
must behave and look exactly as before for every screen outside §43, which means its
footers stay. Comments short and written for the merged state.

## Verification

- Host tests (synthetic only; the repo is public): layout rects and hit-tests for tiles,
  the grid, cards and the segment; region count ordering; footer-height zero on touch
  except Scoring; edge-swipe exclusion; the X4 main-menu equivalence check. Full `ctest`,
  run with
  `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
- `GOLF_CATALOG_DIR=/tmp/golf-catalog-full ctest -R golf_catalog_check`.
- Builds `golf-x4pro`, `golf`, `default` with the installed `~/.platformio`. Keep both
  golf outputs.
- `./bin/clang-format-fix -g` and `git diff --check`.
- **Report:** the §44 reachability table, filled in for **every** golf screen and
  action, with how each was verified.

## Commits (local; no push, no flash)

1. `docs: CONTRACTS-V2 §43 course picker in blocks, §44 footer-less touch` (contract,
   mocks, this brief).
2. `feat: course picker in blocks (§43)`.
3. `feat: no footer on touch boards except Scoring (§44)`.

Commit messages must not carry assistant attribution trailers. Ask rather than invent
product behaviour, especially when a screen has an action §44's table doesn't cover.
