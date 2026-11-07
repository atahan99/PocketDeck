# Third-party code, data, and credits

PocketDeck's firmware is original (MIT, see [LICENSE](LICENSE)). It bundles two
pieces of third-party IR data and borrows ideas from several open projects. Those
keep their own terms, listed here. Where a source's exact license is ambiguous,
check the upstream repo before you redistribute.

## Bundled data

**TV-B-Gone power codes** — `src/tvbgone_codes.cpp`, `include/tvbgone.h`
The canonical Arduino port of Mitch Altman & Limor Fried's TV-B-Gone, with Ken
Shirriff's analysis. Released openly by Adafruit as kit firmware. This is the
Adafruit/Shirriff port, not Bruce's AGPL fork, so no copyleft attaches here.
Upstream: https://github.com/shirriff/Arduino-TV-B-Gone and
https://learn.adafruit.com/tv-b-gone-kit

**Flipper universal "AC off" IR codes** — `include/flipper_ac_off.h`
Community IR captures from the Flipper Zero universal remote set, via the
UberGuidoZ Flipper Infrared collection. These are community-contributed dumps;
see the source repo for its terms.
Upstream: https://github.com/UberGuidoZ/Flipper/tree/main/Infrared

## Libraries

Pulled at build time by PlatformIO (`platformio.ini`), each under its own
license: M5Unified, arduinoFFT, IRremoteESP8266, WiFiManager, ArduinoJson,
NimBLE-Arduino.

## Inspiration (ideas only, no code copied)

The app lineup is a from-scratch rebuild for the StickS3, inspired by the ESP32
hobbyist scene:

- [Bruce](https://github.com/pr3y/Bruce) / [Bruce A-C Edition](https://github.com/tutyr2-jpg/Bruce-A-C-Edition)
- [GhostBLE](https://github.com/SmonSE/GhostBLE)
- [M5Stick-NEMO](https://github.com/n0xa/m5stick-nemo)
- [Open-Wifi-Scanner](https://github.com/geo-tp/Open-Wifi-Scanner)
- [CodeBuddy](https://github.com/CharlexH/CodeBuddy)

Bruce is AGPL; PocketDeck copies none of its code. The overlap is in concepts
(a WiFi scanner, a BLE tool) that the whole scene shares, implemented here on
M5Unified from the ground up.
