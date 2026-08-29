# M1b Review — architect (M1b-1 + M1b-2 + M1b-3)

Reviewed 2026-08-29. Workers: codex (`ctx_0f9b5e0f819b`, `ctx_3677da9d3cad`), then
Claude (`ctx_d75992712f6c`) after codex exhausted its usage limit mid-milestone.

**Verdict: ACCEPTED. First flashable build produced.**

## Gates (architect-run, independently reproduced)

| Gate | Result |
| --- | --- |
| `pio run -e golf` | SUCCESS — `firmware.bin` **5,456,432 bytes** |
| `pio run -e default` | SUCCESS — 5,427,632 bytes, byte-identical to baseline |
| Golf symbols, golf ELF | **164** — the wiring genuinely links |
| Golf symbols, default ELF | **0** |
| Host tests | **48/48** in a clean architect build |
| `clang-format-fix -g` | idempotent |
| Upstream touchpoints | **4 files**: `platformio.ini`, `test/CMakeLists.txt`, `HomeActivity.cpp`, `main.cpp` |
| Flash | 83.0% used, **1.06 MB headroom**. Golf costs **+28.8 KB** |
| RAM | 17.2% (56,404 / 327,680) — **+256 bytes** |

## Two diagnoses I got wrong

I handed M1b-3 a list of four "independent" compile failures. Only two were real:

1. **`ConfirmationActivity.h` — I said it did not exist and had been invented.** It
   exists upstream, tracked since `f42fab1`. The actual fault was a wrong include path
   (`util/…` versus `activities/util/…`). The worker found the real header and fixed the
   path rather than following my instruction to substitute `OptionPopup`, which also
   preserved the proper confirmation dialogs on Finish and Abandon. Had it obeyed me the
   result would have been strictly worse.
2. **The `Rect` ambiguity was not a separate bug at all.** It never reproduced once the
   `GolfStrings::CHANGE` macro collision was fixed — it was a cascade symptom of that one
   root cause.

**The lesson is general.** A single bad preprocessor macro sprays errors that read as
unrelated faults across unrelated files. I treated a compiler's error list as a list of
independent defects instead of asking which single cause could produce all of them. Fix
the macro collision first, then re-read the errors that survive.

Genuinely independent: the `CHANGE` collision, and `getMetrics` being on
`UITheme::getInstance()` rather than `BaseTheme`.

## Wiring, reviewed line by line

`main.cpp` (+16) is correct on both counts that mattered:

* The sleep flush sits immediately after `APP_STATE.saveToFile()` and **before**
  `goToSleep()`, which is upstream's own idiom and covers both sleep paths.
* The resume branch sits **after** `recoveryFirmwareMode` and **after**
  `rebootedFromPanic`. A resumed round can never pre-empt recovery mode or a crash
  report — that was a safety requirement, not a stylistic one, and it is honoured.
* `resumeGolfRound()` returning false on an `archivedAs` file (deleting it, then falling
  through to ordinary routing) is a clean expression of §6.1.

`HomeActivity.cpp` (+21) exceeded my stated ~4-line budget, and the budget was wrong,
not the work: a menu row needs `getMenuItemCount`, the `loop()` dispatch, and `render()`.
Budget corrected in ARCHITECTURE §2.

Worth recording: the worker **declined to add a `HomeMenuItem` enum value** because that
would have touched `ActivityManager.h` as a fifth upstream file, dispatching on index
position instead and documenting why in a comment. That is the §2 tie-breaker applied
correctly without being prompted.

## Process note

Codex exhausted its usage limit mid-milestone. Three replacement launches failed with
`agent_prompt_stalled`, which looked systematic across claude and opencode alike. It was
not: `claude --dangerously-skip-permissions` shows a one-time interactive risk
confirmation that only the owner can clear, and Orca was pasting the dispatch into that
dialog. Creating the terminal first, waiting for the human, then attaching the task with
`worker-start --terminal` worked on the first attempt. Retrying different agents was
wasted effort that obscured a simple unanswered prompt.

## Standing caveat

**Nothing here has run on hardware.** The `RoundArchive` collision loop, every
`HalStorage` path, all four persistence triggers, and the `main.cpp` resume branch are
verified by reading only and cannot be host-tested. See `docs/golf/DEVICE-TEST-PLAN.md`;
the collision test is the highest-value item on it, because a fault there destroys a
completed round.
