# Atom Matrix Four-Mode Firmware

Custom firmware for the [M5Stack Atom Matrix](https://docs.m5stack.com/en/core/ATOM%20Matrix): click the matrix button to cycle modes. Each click shows the destination mode number (1–4), then that mode runs.

> Part of [m5stack-projects](../). Run all commands from this directory (`atom-matrix/`).

| # | Mode | What it does |
|---|------|--------------|
| **1** | Default | Scrolling rainbow |
| **2** | Static | One random solid color or shape; frozen until next click |
| **3** | Animated | Random patterns redrawn ~every 200 ms |
| **4** | Wi‑Fi | One LED per 2.4 GHz AP (max 25). Color = security; brightness = RSSI |

Cycle: 1 → 2 → 3 → 4 → 1 → …

**Boot:** short red prove → digit `1` → rainbow.

### Wi‑Fi LED colors

| Security | Color |
|----------|-------|
| Open | Red |
| WEP | Orange |
| WPA | Yellow |
| WPA2 | Green |
| WPA3 | Cyan |
| Unknown | Magenta |

LED drive capped at `MAX_BRIGHTNESS=20`. No Wi‑Fi credentials (scan-only). No mic on this device — music visualization is out of scope.

---

## Flash without M5Burner

```bash
cd atom-matrix
pio device list          # M5stack → /dev/cu.usbserial-XXXXXXXX
pio run -t upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

Prebuilt merged image (flash at `0x0`):

`atom-matrix/firmware/atom_matrix_tri_mode_1.0.0.bin`

---

## Build

```bash
cd atom-matrix
c++ -std=c++17 -I src -DMAX_BRIGHTNESS=20 test/test_wifi_style.cpp -o /tmp/wifi_style_check && /tmp/wifi_style_check
pio run -t firmware    # → firmware/atom_matrix_tri_mode_1.0.0.bin
```

## Notes

- LEDs: [Adafruit NeoPixel](https://docs.m5stack.com/en/arduino/m5atom/rgb) on GPIO27 (`NEO_GRB`), not `M5.Display` / `M5.Led` (RMT stubs on ESP-IDF 4.x).
- Autodetect may report `board=128` (Atom Lite); NeoPixel path ignores that.
