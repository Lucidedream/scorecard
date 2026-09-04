# v6 — Tips: worker report

Branch: `main` (not committed, per brief). Baseline: 230 golf tests, 15 touchpoints,
golf flash 5,534,041 B (84.4%).

## Status: complete, one item flagged for your call

All four parts built and tested. The one thing needing your attention: the icon
touchpoint question timed out unanswered (`orca orchestration ask`, thread
`msg_4acea56b26d5`, 30-min timeout), so I proceeded on the path the brief itself
recommends — a **separate golf icon header** rather than editing
`src/components/icons/listIcons.manifest`. Rationale below; easy to revisit.

---

## 1. Note storage and format (§25.1)

- `src/golf/GolfTips.{h,cpp}` — streaming parsers, no firmware deps, host-tested.
  - `GolfTipScanner` — feeds bytes, yields the **title (line 1)** and **section count**.
  - `GolfTipSectionReader` — feeds bytes, materialises **one section by index** into a
    caller-owned `GolfTipSection`; walks the sections before/after without storing them
    so the total count is still known at `finish()`.
  - `golfTipsListState(directoryError, fileError, noteCount)` — pure policy, the
    §22.4-style three-way split (Ready / Empty / Error).
- `src/golf/GolfTipsStore.cpp` — SD-facing (`#if CROSSPOINT_GOLF`, `HalStorage`).
  Scans `/golf/tips` exactly the way `CourseStore` scans `/golf/courses`: one `HalFile`
  at a time, `Storage.open` + `openNextFile`, `.txt` filter, per-file streamed through
  `GolfTipScanner` in 128-byte chunks. Split from `GolfTips.cpp` so the host suite can
  build the parsers without stubbing `HalStorage` (same split as
  `GolfRoundFile.cpp` / `GolfRoundDecode.cpp`).
- Format rules implemented exactly as written: line 1 = title; blank line starts a
  section; section line 1 = heading; leading `•` (U+2022, 3 bytes) or `-` = bullet
  (marker + one optional space stripped), anything else = paragraph. CRLF, lone CR,
  multiple blank lines between sections, and a missing trailing newline all handled.
- `docs/golf/examples/tips/slope-strategy.txt` copied to
  `test/golf_tips/fixtures/slope-strategy.txt` and parsed in the tests:
  **9 sections, per-section bullet counts {5,4,5,4,4,4,4,4,3} = 37, title "Slope
  Strategy" from line 1, 2152 bytes.** All asserted.

## 2. The note reader pages by section (§25.2)

- `src/activities/golf/GolfTipNoteActivity.{h,cpp}` — plain `Activity`, custom render.
  Left/Right and the side rocker (`PageBack`/`PageForward`) turn pages, wrapping at the
  ends like hole review; Back exits; Confirm does nothing (footer shows `— `).
- Page unit is the **section**: heading band at top (2 px rule under it), bullets
  beneath, `N / M` position counter pinned to the bottom above a 1 px rule.
- **Body font: `NOTOSANS_18` (regular), heading `NOTOSANS_18` bold.** This is the
  largest prose face in the build — its line box is 51 px against `UI_12`'s 29 px used
  everywhere else in the app. The mock's 19 px sits between `NOTOSANS_16` and `_18`;
  `_18` wins on legibility and the **leading is tightened to 0.80 (≈41 px/line)** so the
  densest section still fits. Heading leading 0.92 (≈47 px).
- **Overflow is measured, then marked, never silently truncated.** `drawSection()` does
  a measure pass (wrap every line, sum block heights, a bullet's whole wrapped block
  must fit or it is not started — bullets are never split across a break). If the
  section does not fit, OR the store already had to cap it at
  `GOLF_TIP_SECTION_MAX_LINES` (10), an **inverted full-width bar** is drawn at the
  bottom of the bullet area reading *"Section too long for one screen — trimmed"*
  (`STR_GOLF_TIP_SECTION_TRIMMED`), and only the lines that fit above it are drawn.
- **Only the current section is resident.** The file is read in 128-byte chunks and
  never held whole; the tips folder is never enumerated past the bounded 24-entry list.
  See allocation numbers below.

## 3. Two entry points, same screens (§25.3)

- `src/activities/golf/GolfTipListActivity.{h,cpp}` — `UiListActivity`. One row per
  file: **title = the note's first line** (filename stem as fallback when line 1 is
  empty), subtitle = *"N sections"*. Empty state
  (`STR_GOLF_TIPS_EMPTY`) and unreadable state (`STR_GOLF_TIPS_LIST_ERROR`) are kept
  distinct, as §22.4 requires. A bottom hint line names `/golf/tips`.
- **Main menu:** Tips is now a **fourth tile** in `GolfHomeActivity`
  (`Destination::Tips`), reached with `startActivityForResult`, Confirm hint "Open".
  Tile row height dropped 120 → 112 px for the four-wide layout. Detail panel reports
  the note count (`STR_GOLF_TIPS_COUNT_ONE` / `_COUNT_FORMAT` / `_NONE` /
  `_UNAVAILABLE`), probed on entry via `GolfTipsStore::enumerate(nullptr, …)` — a
  cheap count-only pass that opens no files.
- **Round menu:** order is now **View card, Abandon round, Finish round, Tips**
  (`GolfRoundMenuActivity`, `ROW_COUNT = 4`). "Finish round" wording kept. Tips reaches
  the same `GolfTipListActivity`.
- **§22.1 updated** in `CONTRACTS-V2.md` — the "fixed at three" claim now carries an
  "Amended by §25.3 (v6)" block: the set is four, the reasoning was always
  small/fixed/fully-visible, four at 118 px still satisfies it, four is the ceiling.

## 4. Tile icons (§25.4)

- `land-plot` / `scroll-text` / `trending-up` / `lightbulb`, generated **through the
  existing pipeline** (`gen_icons.py`, sizes 24 and 32) from a Lucide checkout
  (cloned `lucide-icons/lucide`, ISC). Tooling installed on this machine to run it:
  `brew install librsvg`, `pillow` into `.venv`.
- **Written to a NEW file, `src/components/icons/golfTileIcons.{manifest,h}`, not to
  the upstream `listIcons.manifest`.** `listIcons.manifest` and `listIcons.h` are pure
  upstream files the fork has never modified; adding to them is a new touchpoint over
  the FIFTEEN budget and needs the bounded-touchpoint sign-off. A separate header in
  the already-golf-adjacent `src/components/icons/` (exactly like the existing
  `scorecard.h`) touches no upstream file. The generated header is committed; the SVGs
  are not (Lucide is generation-time only). **This is the STOP-AND-ASK item — the ask
  timed out, so I took the brief's own recommended path. Revert to editing
  `listIcons.manifest` if you'd rather spend the touchpoint.**
- **Tiles use the 32 px icon** (drawn icon-above-label; the stock `button()` lays icon
  and label side-by-side, which will not fit "New round" + a glyph in 118 px, so the
  tile draws the box/hit/selection via `screen.button` and then the icon + wrapped
  centred label manually, inverting to white on the selected tile). 32 px in a ~118 px
  tile with ~16 px top padding and a 2-line label below reads comfortably per the
  layout math; **on-device confirmation is a human-tester step.**

## Acceptance

| # | Result |
|---|---|
| 1 | `pio run -e golf` **SUCCESS**. Flash **5,545,405 B** (84.6%), **+11,364 B / +0.205%** vs the 5,534,041 B baseline. RAM 17.4%. |
| 2 | `pio run -e default` **SUCCESS**. `nm firmware.elf | grep -i golf` → nothing; only `ScorecardIcon` present. |
| 3 | **408 host tests pass** (was 398), **240 golf** (was 230). New suite `test/golf_tips` (10 tests): slope-strategy → 9 sections + bullet counts + 37 total; title from line 1; overflowing section marked (`overflow` set, `lineCount` capped, first/last kept); empty vs unreadable folder distinguished; plus dash/bullet/paragraph classification, multiple blank lines, CRLF, missing-section-not-found. |
| 4 | `PATH=/opt/homebrew/opt/llvm/bin:$PATH ./bin/clang-format-fix -g` — **tree unchanged** (verified idempotent; new files were `git add -N`'d so the wrapper sees them). |
| 5 | **Touchpoints: 15, unchanged.** Only two upstream files touched, both pure appends within their designated touchpoint: `test/CMakeLists.txt` (+1 `add_subdirectory(golf_tips)` below the `# --- golf (fork) ---` marker) and `lib/I18n/translations/english.yaml` (+14 `STR_GOLF_*` keys). `listIcons.manifest`/`.h` and upstream `HomeActivity.cpp` untouched. |
| 6 | **Peak allocation while reading a note** (worst instant = a page turn with the list still on the stack): note-list scratch `GolfTipEntry[24]` + `ListItem[24]` + subtitle buffers ≈ **~4.0 KB**, plus the reader's resident `GolfTipSection` (**1,346 B**) plus one transient staging `GolfTipSection` during the turn (**1,346 B**) → **≈ 6.7 KB peak**, ≈ 5.4 KB steady while reading. Transient stack in `readSection`: `GolfTipSectionReader` ~160 B + 128 B chunk. **The whole file is never resident** — read in 128-byte chunks, only the current section's ≤10 parsed lines held; the tips directory is never fully loaded. |
| 7 | **Vertical fit, densest section (Downhill lie, 5 bullets, 4 wrapping to 2 lines).** Computed from theme metrics (topPadding 5, golf header 46, button hints 40) and font advanceY: bullet area ≈ **576 px** on an 800 px portrait screen; the section needs ≈ **409 px** at `NOTOSANS_18` / 0.80 leading (41 px/line, 10 px between bullets) → **≈ 167 px / ~4 lines of headroom**. Even at the font's full 51 px leading it fits (≈ 499 px). Device confirmation is still a human-tester step (DEVICE-TEST-PLAN.md). |

## Files

New: `src/golf/GolfTips.{h,cpp}`, `src/golf/GolfTipsStore.cpp`,
`src/activities/golf/GolfTipListActivity.{h,cpp}`,
`src/activities/golf/GolfTipNoteActivity.{h,cpp}`,
`src/components/icons/golfTileIcons.{manifest,h}`,
`test/golf_tips/{CMakeLists.txt,test_golf_tips.cpp,fixtures/slope-strategy.txt}`.

Modified: `src/activities/golf/GolfHomeActivity.{h,cpp}`,
`src/activities/golf/GolfRoundMenuActivity.{h,cpp}`,
`docs/golf/CONTRACTS-V2.md` (§22.1), `lib/I18n/translations/english.yaml`,
`test/CMakeLists.txt`.

Untracked and left for you: `docs/golf/design/tips.html`,
`docs/golf/design/tile-icons.html`, `docs/golf/examples/tips/slope-strategy.txt`
(the design + owner note you added).

## Notes / smaller decisions

- Pager and footer use "N / M" and "Prev"/"Next" text, not `◂`/`▸` glyphs — golf
  strings have never used arrow glyphs (they're not vetted in the device fonts, unlike
  `•`), and the home-menu footer already reads "Prev"/"Next".
- The round-menu "Hole 7 · Thru 6 · 3 notes" strip in the mock is not implemented —
  it's not in §25.3 or acceptance, and the round menu is a plain list. Say if you want it.
- `GOLF_MAX_TIPS = 24` (the list scratch is ~4 KB at that cap). Bump or shrink freely;
  the enumerate loop marks `overflow` past the cap.
- Home-tile detail is a single count line ("N notes on the SD card."), not the mock's
  two-line "1 note · Slope Strategy, 9 sections." — §25.3 only asks for the count.
