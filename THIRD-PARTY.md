# Third-party code and licenses

PixelDeck's own code is MIT (see [LICENSE](LICENSE)). The firmware binary also
contains the libraries below, which PlatformIO fetches at build time at the
versions pinned in `platformio.ini`. Each keeps its own license.

| Component | Version | License | Upstream |
|-----------|---------|---------|----------|
| Adafruit NeoPixel | 1.15.5 | LGPL-3.0-or-later | https://github.com/adafruit/Adafruit_NeoPixel |
| M5Unified | 0.2.29 | MIT | https://github.com/m5stack/M5Unified |
| M5GFX (pulled in by M5Unified) | 0.3.0 | MIT | https://github.com/m5stack/M5GFX |
| Arduino-ESP32 core | 2.0.16 (platform espressif32@6.7.0) | LGPL-2.1-or-later | https://github.com/espressif/arduino-esp32 |
| ESP-IDF | 4.4.x (bundled with the core) | Apache-2.0 | https://github.com/espressif/esp-idf |

## LGPL note (prebuilt binaries)

Adafruit NeoPixel and the Arduino-ESP32 core are LGPL and are statically linked
into the release `.bin`. The LGPL lets you swap in a modified version of those
libraries and relink. PixelDeck meets that by publishing its complete source and
build setup: change the library (for example by pointing `lib_deps` at your
fork) and run `pio run -t firmware` to get a relinked image. Each library's full
license text ships in its upstream repository.
