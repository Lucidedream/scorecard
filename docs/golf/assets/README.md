# Scorecard sleep wallpaper

`sleep.bmp` is the device-ready Scorecard sleep wallpaper for the Xteink X4. It is a 1-bit (pure black and white) BMP at the X4's native 480x800 portrait resolution. Thresholded from the original grayscale artwork rather than dithered, since the mark is flat background plus fine linework, not a photo. Pure black and white avoids the fading the 4-level grayscale waveform shows on content left on screen for a long time (the sleep screen's whole reason to exist) -- 1-bit is the panel's native, fully stable state.

## Install

1. Copy `sleep.bmp` to the root of the device's SD card as `/sleep.bmp`.
2. On the X4, select **Settings -> Display -> Sleep Screen -> Custom**.

This wallpaper is separate from the Scorecard app logo master in `src/golf/assets/`. The wallpaper remains SD-loaded rather than embedded in firmware, avoiding a full-screen image payload in firmware flash. The original high-resolution PNG is intentionally not duplicated here because the device-ready BMP is sufficient for deployment.
