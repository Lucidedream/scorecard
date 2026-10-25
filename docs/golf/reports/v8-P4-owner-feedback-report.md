# P4 owner feedback: tile geometry and player counter

This follow-up implements the owner amendments to CONTRACTS-V2 §43.2, §43.5 and §44 after device testing. It supersedes the initial P4 report's four-row Count behavior and tile-count subtitles.

The main menu, History chooser, picker hub and Course info now use `golfui::takeTileStrip`, whose pure layout caps the strip at the original 112 pixels. All four use the same `tileGeometry` and label-only drawing; the subtitle argument and drawing path have been removed. Recent's detail box starts with its translated course count and permits five lines for that count plus four course previews. My courses and By region retain their detail counts. No new heap allocation or framebuffer was introduced.

Player setup's Count phase uses the original centered counter. Its footer opt-in is passed through the shared list's footer input/drawing and both chrome adapters, so Back, Next, Previous and Next are touch targets and reserve exactly the original space. The Players and Tee choice phases continue without a touch footer, and the roster keeps its Start round last row. Non-touch rendering and input retain their existing behavior.

| Changed surface | Actions / touch reachability | Verification |
|---|---|---|
| Picker hub | Tap Recent, My courses or By region; counts appear only in the detail box; edge Back/Home retain host handling | Shared strip/geometry tests; source inspection |
| Course info | Tap Play, Hole maps or Tee info; edge Back/Home retain host handling | Same shared strip/geometry tests; source inspection |
| Player setup Count | Centered counter; footer Previous/Next adjust 1–4; footer Next/Confirm opens Players; footer Back exits; side controls remain | Phase/footer layout and four-cell hit tests; source comparison with pre-§44 commit `ef557deb` |
| Players roster / Tee choice | Tap player/tee/name rows; Start round is the roster's final row; edge Back returns to the previous step | Existing handlers retained; footer policy tests check both phases |

The centered counter drawing, Count footer labels, and footer Confirm/Previous/Next handlers compare exactly with `ef557deb`, before the §44 implementation. The X4 main-menu equivalence test also remains passing.

Validation:

- Full host build and `ctest`: 566 passed, with the environment-gated `golf_catalog_check` skipped in that run (567 registered tests; no failures).
- `GOLF_CATALOG_DIR=/tmp/golf-catalog-full ctest -R golf_catalog_check`: passed separately.
- Both host commands used `CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
- Formatting used `./bin/clang-format-fix -g`; `git diff --check` passed.
- Firmware `golf` and `golf-x4pro` both passed after the last code edit; both outputs are retained in ignored `build/golf-p4/` and `.pio/build/`.

Logs: `/tmp/golf-p4-feedback-host-build.log`, `/tmp/golf-p4-feedback-ctest.log`, `/tmp/golf-p4-feedback-catalog-check.log`, `/tmp/golf-p4-feedback-firmware.log`.

No push or flash was performed. Human verification remains: compare the hub and Course info icon/label spacing with the main menu, check all Count footer cells and the transition to a footerless roster, and confirm unchanged X4 behavior on device. No runtime heap improvement is claimed.

| Firmware | Retained output | SHA-256 |
|---|---|---|
| golf | `build/golf-p4/golf.bin` | `94f46ba00999211a1b1dae196db5fddc666ac3c31f26d1deaeccd8f27ca253f0` |
| golf-x4pro | `build/golf-p4/golf-x4pro.bin` | `bad2e6be90cefb8fa47b7cbce9b0a5e8afecd27bf1123863324c2de2972502e5` |
