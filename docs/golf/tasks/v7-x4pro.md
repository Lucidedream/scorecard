# v7 — Scorecard on the Xteink X4 Pro

**Worker task brief.** `docs/golf/CONTRACTS-V2.md` §37 is the specification and is
authoritative; read it in full first, then `docs/golf/ARCHITECTURE.md` §2 (the
bounded-touchpoint rule). Root `AGENTS.md` / `CLAUDE.md` govern coding standards. You do
not amend the contract: if it is wrong or impossible, stop and ask the coordinator.

## Goal

The owner has an X4 Pro (ESP32-S3, touch, two side keys + Power). Make the existing
Scorecard app build for it and make every golf screen operable with the inputs that
device has. No new feature; nothing changes on the X4.

You have no hardware. Acceptance is green builds, passing host tests and review.

## Work on

Branch `Lucidedream/xteink-pro`, in this worktree. Commit locally in logical commits
(`feat:` / `test:` / `docs:`), no assistant co-author trailer, **never push**.

## Deliverables

1. **`[env:golf-x4pro]`** in `platformio.ini` per §37.2.
2. **Touch footer** per §37.3: constexpr geometry + hit-test in
   `src/activities/golf/GolfUiLayout.h`, `golfui::drawFooter(...)`, the golf-side input
   helper, and the layout reservation. Replace all golf `drawButtonHints` /
   `drawButtonHintsTwo` / `drawSideButtonHints` call sites.
3. **Scoring** per §37.4 (tap-to-focus a counter band; mark sheet row taps and
   tap-outside cancel), including the written Power click / double-click finding.
4. **Every other golf activity** per §37.5.
5. **Host tests** in `test/golf_layout` for the footer geometry and hit-test.
6. **Report** at `docs/golf/reports/v7-x4pro-worker-report.md` containing: the §37.5
   reachability table (one row per activity, every action, the Pro input reaching it);
   the Power-click finding; exact build/test commands run with their results; firmware
   size of both builds; anything you could not verify.

## Hard constraints

* The only upstream (non-golf) file you may edit is `platformio.ini`. Not `src/main.cpp`,
  not `MappedInputManager.*`, not themes, not `lib/hal`, not `freeink-sdk`.
* Gate at runtime on `mappedInput.hasTouch()`, not `#if FREEINK_CAP_TOUCH`. Confirm each
  touch API you call is declared unconditionally in `src/MappedInputManager.h`.
* With `hasTouch()` false, rendering and input are behaviourally identical to today.
* No new heap allocation; locals under 256 bytes; UI strings through `tr()`.
* Touch queries are per-frame state: make sure one tap cannot fire two actions (footer
  before `routeTouch`, return after a hit) and that a long-press does not also deliver a
  tap on release (`wasScreenLongPress` semantics, `MappedInputManager.h:57-63`).

## Verify (acceptance, §37.6)

```bash
~/.platformio/penv/bin/pio run -e golf-x4pro
~/.platformio/penv/bin/pio run -e golf
cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j
./bin/clang-format-fix -g
git diff --stat main... -- . ':!src/activities/golf' ':!src/golf' ':!test' ':!docs'   # expect platformio.ini only
```

`pio` is not on PATH; use the path above. Disk space on this machine is tight (~10 GB):
do not run `pio run -t clean` followed by rebuilds of other environments, and do not
build any environment beyond the two listed.
