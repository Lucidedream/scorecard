# Scorecard — project status

Last updated 2026-08-31, after syncing the other agent's work at `0ba2b98`.

## Where it stands

The app is **feature-complete for its stated purpose** and running on the owner's device.

| | |
| --- | --- |
| Base | CrossPoint fork, rebased onto `upstream/develop` |
| Golf host tests | **128** across 13 suites |
| Flash | **83.6%** of the 6,553,600-byte app slot — ~1.07 MB spare |
| Upstream touchpoints | **6**, held since the first milestone |
| Courses | Sanyang (Blue + White), MoganShan Gowin, Pebble Beach, Template (par-free) |

## What it does

**Scoring.** One-handed: Up/Down counts, Power cycles the field, Confirm adds a penalty.
Three metrics per hole — putts, inside 100, scoring zone — with the total derived. Holes
pre-seed to par, and the press that would wrap focus commits and advances instead.

**Penalties.** Hazard (+1) and OB (+2, USGA Local Rule E-5) with per-field markers, a
`PENALTY +N` band, and Down-to-remove. Penalties are a fourth bucket, so the long-game
figure is honest for the first time.

**Persistence.** Round state survives deep sleep; waking resumes into the current hole.
Rounds archive to `/golf/rounds/` with a sequence-numbered filename and an `index.csv`
row. Dates arrive on their own from an SNTP sync on Wi-Fi connect.

**Review.** History lists rounds newest-first from `index.csv` alone. Selecting one opens
a menu: a three-row scorecard, read-only hole-by-hole review, per-round statistics, and
delete. Cross-round trends fold scoring, putting and the bucket mix.

## Recent work by the other agent

Four commits, all verified here as building clean with 128 tests passing and six
touchpoints held:

* `60959ba` — hole review also accepts the side rocker (`PageBack`/`PageForward`)
  alongside Left/Right. Sensible: the side buttons are the natural page-turn gesture on
  this hardware.
* `b7e0498` / `99c67b8` — Scorecard logo, icon rotation fix, and an SD-loaded sleep
  wallpaper. The wallpaper stays on the SD card rather than in firmware, which is the
  right call: a 480x800 image would be a large flash payload for a decorative asset.
* `0ba2b98` — **the golf home menu is now a flat five rows** (Scorecard, File Browser,
  Recents, File transfer, Settings) with the recent-book cover tile removed under
  `#if !CROSSPOINT_GOLF`.

That last one deserves a note. It replaces the v3.2 selector-shift machinery
(`kHomeSelectorShift`, `homeUpstreamSelector`) with a straight `selectorIndex == 0`
check, and shrinks `HomeActivity.cpp`'s diff against upstream from **+119/-13 to
+86/-13** — a real reduction in the fork's largest rebase liability. Losing the cover
tile costs one keypress to reach a book via Recents, and buys a simpler home and a
smaller diff. Verified: the non-golf build is untouched, so the stock reader keeps its
cover tile.

## Open, needing the device rather than more code

* **The no-tabs card.** Two stacked nines measure ~376 of 671 available px — 295px
  spare — so removing the Front/Back tabs would fit. Whether it *reads* better is a
  panel judgment, not a measurement.
* **Dates have never run on hardware.** Deep-sleep persistence is verified against
  Espressif's C3 documentation and this build's config flags
  (`CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=1`), which is as far as verification goes without
  a device. First real confirmation is a round finished after a Wi-Fi sync.
* **Delete against real data.** The staged index rewrite is host-tested, but has not run
  against the owner's actual `index.csv`.

## Not built, deliberately

The on-device course editor — the last item on the original plan — was dropped in favour
of penalty tracking. Authoring a course JSON on a computer takes a minute; entering 18
pars through stepper rows does not. The upload path already works.
