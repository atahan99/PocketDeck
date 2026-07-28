# StickS3 — merged launcher firmware

Eleven apps in one flashable firmware for the
[M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO N8R8,
8 MB PSRAM). No SD card — everything is compiled in and picked from a carousel
launcher. Native M5Unified, from scratch (not a port of the StickC firmwares,
which boot blank on this board).

> Part of [m5stack-projects](../). Run commands from `stick-s3/`.

![Carousel launcher](docs/images/launcher.png)

## Apps

| App | What it does |
|-----|--------------|
| **WiFi Scan** | 2.4 GHz APs: SSID, RSSI, channel, encryption. |
| **Spectrum** | Promiscuous channel monitor (1–13) + live packet HUD. |
| **Audio Viz** | ES8311 mic → FFT bars. BtnA cycles gain. |
| **IR Tools** | TV-B-Gone, AC-B-Gone (Flipper `ac.ir` codes), Universal Remote (learn on G42 → replay on G46, saved to NVS), Emit Test. |
| **AI Buddy** | Claude Code / Codex / Cursor status, pushed from a host over WiFi or BLE. |
| **BLE Scan** | Passive nearby-device scan. |
| **BLE Spam** | Pairing-popup advertising spam (Apple/Windows/Samsung/Android). Warning-gated, authorized/own-device use only. |
| **Time** | Stopwatch, Timer (BtnB +1 / double-click −1), Pomodoro, Interval. Offline. |
| **Light** | Full-white flashlight. |
| **Toys** | Tetris, Snake, Flappy, Dino, Breakout, Tilt Maze (IMU). |
| **Settings** | WiFi (off by default; toggle + setup), brightness, Grove 5V, about. |

**Controls.** Two buttons: **BtnA** = select/action, **BtnB** = next, **long-press**
= back. Battery % shows in every header; the screen dims then sleeps on idle (on
battery only, to keep USB flashing alive), and holding power sleeps on demand.

## Screenshots

![App gallery](docs/images/gallery.png)

## Build & flash

Needs [PlatformIO](https://platformio.org/); the first build pulls the S3 toolchain.

```bash
./scripts/flash.sh            # auto-detects the port
```

Platform pinned to **pioarduino 55.03.311** (arduino-esp32 3.3.11) — see
[gotchas](#gotchas). `firmware/stick-s3.bin` ships no secrets (WiFi is entered
on-device).

### Drive it from a host

`scripts/stick.py` works the UI over USB serial and returns the screen as a PNG:

```bash
python scripts/stick.py shot            # -> shot.png
python scripts/stick.py key b b a       # BtnB, BtnB, BtnA
python scripts/stick.py logs -s 20      # stream the log
```

`scripts/buddy.py` feeds the AI Buddy app — it tails Claude/Codex/Cursor session
logs and pushes status over WiFi (`wifi`) or BLE (`ble`, needs `pip install bleak`).

## Gotchas

- Prebuilt StickC firmwares boot blank — StickS3 is new; needs M5GFX ≥ 0.2.16.
- Octal PSRAM needs `board_build.arduino.memory_type = qio_opi` or the framebuffer alloc fails.
- Mic (ES8311) needs arduino-esp32 ≥ 3.2.0 (older → GDMA error); BLE needs NimBLE ≥ 2.x (1.4 hangs at init).
- **IR is powered from the EXT_5V rail**, which also coil-whines at idle — so it's off by default and switched on only inside IR Tools. That's why IR is dead elsewhere and buzzes while in use.
- Backlight PWM is moved 256 Hz → 44.1 kHz to stop an audible whine.
- Tilt Maze mirrored? Flip `TILT_SIGN` in `toys.cpp`.

## Inspiration & credits

App ideas from the ESP32 hobbyist scene (from-scratch rebuild, not a port):
[Bruce](https://github.com/pr3y/Bruce), [GhostBLE](https://github.com/SmonSE/GhostBLE),
[M5Stick-NEMO](https://github.com/n0xa/m5stick-nemo),
[Open-Wifi-Scanner](https://github.com/geo-tp/Open-Wifi-Scanner),
[CodeBuddy](https://github.com/CharlexH/CodeBuddy).

TV-B-Gone codes: the Adafruit / Ken Shirriff / Mitch Altman port
(`src/tvbgone_codes.cpp`). AC OFF codes: Flipper Zero's universal
[`ac.ir`](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/applications/main/infrared/resources/infrared/assets/ac.ir)
(`include/flipper_ac_off.h`).
