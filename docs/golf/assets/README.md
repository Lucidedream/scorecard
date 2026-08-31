# Scorecard sleep wallpaper

`sleep.bmp` is the device-ready Scorecard sleep wallpaper for the Xteink X4. It is an uncompressed 24-bit RGB BMP at the X4's native 480x800 portrait resolution.

## Install

1. Copy `sleep.bmp` to the root of the device's SD card as `/sleep.bmp`.
2. On the X4, select **Settings -> Display -> Sleep Screen -> Custom**.

This wallpaper is separate from the Scorecard app logo master in `src/golf/assets/`. The wallpaper remains SD-loaded rather than embedded in firmware, avoiding a full-screen image payload in firmware flash. The original high-resolution PNG is intentionally not duplicated here because the device-ready BMP is sufficient for deployment.
