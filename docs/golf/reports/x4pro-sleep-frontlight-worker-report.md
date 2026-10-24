# X4 Pro deep-sleep frontlight fix

Implemented in three firmware HAL files (59 added lines), with no SDK modifications, settings writes, heap allocations, flashing, commits, or pushes.

Changes and evidence:
- `lib/hal/HalPowerManager.cpp:70–71` calls `Frontlight.prepareForDeepSleep()` before serial teardown. This covers normal sleep and both direct setup sleep paths at `src/main.cpp:312`, `:395`, and `:471`.
- `lib/hal/HalFrontlight.cpp:36–70` sets SDK brightness to zero (both PWM channels and RC_FAST release), stops the SDK light-sleep LEDC channels at the board-specific OFF level, selects plain GPIO outputs, drives OFF, and enables pad holds. It skips PM1/I2C lights and compiles to a no-op without FREEINK_CAP_FRONTLIGHT. The initialization flag avoids stopping uninitialized channels during rejected early wake.
- `lib/hal/HalFrontlight.cpp:17–28` releases surviving pad holds before LEDC initialization on wake.
- `lib/hal/HalFrontlight.h:12,33` declares the method and adds one boolean initialization flag. No new heap allocation or buffer was introduced.
- SDK diagnosis verified: `freeink-sdk/libs/hardware/FrontlightManager/src/FrontlightManager.cpp:61–65` owns channels 0/1; `:85–108` configures RC_FAST and KEEP_ALIVE; `:311–328` balances the sleep-clock request when brightness becomes zero. `BoardConfig.h:1576` specifies active-high GPIO8/9 and `:1605` specifies master latch GPIO1. `PowerManager.cpp:93–96` applies GPIO isolation and deep-sleep holds.
- `SETTINGS.frontlightOn` and wake restore logic at `src/main.cpp:449–453` are unchanged; GPIO1 power-latch behavior is unchanged.

Verification:
- PASS: `/Users/noah/.platformio/penv/bin/pio run -e golf-x4pro` after reclaiming this task's build cache. Firmware remains at `/Users/noah/orca/workspaces/scorecard/x4pro-sleep-frontlight/.pio/build/golf-x4pro/firmware.bin` (ELF alongside it).
- Final successful log: `/tmp/x4pro-sleep-frontlight-golf-x4pro-retry.log`; elapsed 120.54 seconds. Static RAM 90,628 / 327,680 bytes; flash 5,512,106 / 6,553,600 bytes. These are build sizes, not runtime heap measurements.
- One warning from unchanged dependency `.pio/libdeps/golf-x4pro/WebSockets/src/WebSocketsClient.cpp:573`: deprecated NetworkClient::flush(). No warnings from modified HAL sources.
- PASS: `./bin/clang-format-fix -g` and `git diff --check`.
- INCOMPLETE: `x4pro` and C3 `golf` builds were attempted but hit disk-full errors. They were not retried after the coordinator explicitly deferred all checks except golf-x4pro.
- INCOMPLETE: `cmake -S test -B build/test` configured successfully; the initial host compile failed because Apple CLT did not find `<cstddef>`. Supplying `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1` allowed compilation to 85%, then the coordinator directed stopping host work and deleting its build outputs to prioritize firmware. CTest was not run.
- Initial overlapping PlatformIO runs invalidated output directories, so the successful final target ran alone. Disk-full failures also occurred during verification; only this task's own generated build directories/cache were removed, with coordinator authorization.

Remaining device verification: measure sleep current and GPIO8/9 OFF levels after sleeping with the light on at cool, mixed, and warm settings, and compare to sleeping with the light already off. Repeat wake/sleep cycles with Restore Light on Wake enabled and disabled, checking that saved brightness/warmth and frontlight preference remain intact; test rejected short power-button wake as well. Device testing and runtime heap measurement remain with the coordinator/user.

The pre-existing untracked task document `docs/golf/tasks/x4pro-sleep-frontlight.md` was left unchanged.

## Follow-up verification

After disk space was freed, the remaining checks were run on the same patch without
code changes: `x4pro` PASS, C3 `golf` PASS, host CTest 544/544 PASS. Applied on top of
course nines (§40) as the combined firmware, `golf-x4pro` and `golf` both PASS; that
build was flashed to an X4 Pro and an X4 and both boot cleanly. Overnight sleep-current
comparison is still pending on hardware.
