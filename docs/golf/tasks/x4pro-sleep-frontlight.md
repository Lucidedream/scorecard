# Task: X4 Pro frontlight must be fully off through deep sleep

Branch/worktree: `Lucidedream/x4pro-sleep-frontlight` at
`/Users/noah/orca/workspaces/scorecard/x4pro-sleep-frontlight` (based on origin/main 7c41a79c).
First run `git submodule update --init freeink-sdk` — the submodule is not checked out yet.

## Symptom

The X4 Pro (`pio run -e golf-x4pro`, also `x4pro`) drains its battery quickly **while asleep**.

## Diagnosis (verify each point before relying on it)

- `src/main.cpp:263-313` `enterDeepSleep()` tears down Wi-Fi, tilt sensor and display, then
  `HalPowerManager::startDeepSleep()` (`lib/hal/HalPowerManager.cpp:69-133`) holds the power
  latches HIGH and cuts the touch/SD rails via `freeink::PowerManager::powerDownRailsForSleep()`.
  **Nothing turns the frontlight off.** `startDeepSleep()` is also called directly from
  `src/main.cpp:395` and `:471` — all three paths must be covered.
- The X4 Pro envs build with `-DFREEINK_FRONTLIGHT_LS`
  (`freeink-sdk/libs/hardware/FrontlightManager/src/FrontlightManager.cpp:72-107,169-183,311-329`):
  LEDC channels on GPIO8 (cool) / GPIO9 (warm), RC_FAST clock, `LEDC_SLEEP_MODE_KEEP_ALIVE`, pad
  sleep-isolation override disabled, and `esp_sleep_sub_mode_config(ESP_SLEEP_DIG_USE_RC_FAST_MODE,
  true)` held while lit. If the light is on at sleep time, RC_FAST stays requested and GPIO8/9 are
  exempt from `esp_sleep_config_gpio_isolate()`, so the pins can sit at an undefined/partly-on level
  for the whole sleep. Board profile: `freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h`
  `XTEINK_X4_PRO` frontlight `{8, 10000, 10, true, 9}` (active-HIGH).
- GPIO1 (power.latch0, master rail) stays HIGH through sleep by design — do not change that.

## Required change

1. Before deep sleep, on every `startDeepSleep()` path, put the frontlight hardware fully off:
   - duty 0 on both channels through the existing API so `updateLsKeepAlive(false)` releases the
     RC_FAST sub-mode refcount;
   - stop the LEDC channels at the OFF idle level and drive GPIO8/GPIO9 as plain outputs at the
     OFF level (active-HIGH ⇒ LOW), then `gpio_hold_en()` each so the level survives
     `esp_sleep_config_gpio_isolate()` + `gpio_deep_sleep_hold_en()`. Follow the existing
     `gpio_hold_dis` → drive → `gpio_hold_en` pattern in `HalPowerManager.cpp`.
   - Use pins from `BoardConfig::ACTIVE.frontlight` (skip `PIN_UNASSIGNED`); no-op on boards
     without `FREEINK_CAP_FRONTLIGHT` or with a PM1/I2C frontlight (only GPIO PWM lights).
2. Do **not** change `SETTINGS.frontlightOn` — `src/main.cpp:452` restores the light on wake from
   it when `frontlightRestoreOnWake` is set; that must keep working.
3. Pad holds survive the deep-sleep reset. Release them (`gpio_hold_dis` on both frontlight pins)
   before `Frontlight.begin()` reattaches LEDC, otherwise the light can never turn on after wake.
4. Prefer implementing in this repo's HAL (`lib/hal/HalFrontlight.*` and/or
   `lib/hal/HalPowerManager.cpp`). Do **not** modify the `freeink-sdk` submodule (that would
   need a separate SDK commit); if you find it is impossible without an SDK change, stop and ask.
5. C3 boards (X4/X3) must be unaffected — they have no frontlight.

Follow CLAUDE.md: LOG_* not Serial, keep comments short and written for the merged state, no new
heap allocation, cite file:line in your report.

## Verification

- `~/.platformio/penv/bin/pio run -e golf-x4pro` and `-e x4pro` and `-e golf` (C3) all build with
  no new warnings. PlatformIO is not on PATH.
- Host tests: `cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test`.
- `./bin/clang-format-fix -g`.
- Do **not** flash, commit or push. Report the diff summary, build/test results, and anything you
  could not verify; the coordinator will flash and the user will measure sleep drain.
