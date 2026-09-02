# Firmware builds

Prebuilt Scorecard firmware for the Xteink X4, for flashing without a toolchain.

| File | Branch | Commit |
| --- | --- | --- |
| `scorecard-v5-multiplayer.bin` | `qiliu4/multiplayer` | `e62f7a4` |

## Flashing

Copy the `.bin` to the SD card and use **Settings → Firmware update** on the device.

## A note on size

These are ~5.3 MB each, and git keeps every version forever — replacing the file does
not reclaim the space its predecessor took. Keep one current build here rather than a
history of them; older builds can always be rebuilt from their commit with
`.venv/bin/pio run -e golf`.
