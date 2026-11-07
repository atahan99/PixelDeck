#pragma once

#include <stdint.h>

// Arduino ESP32 wifi_auth_mode_t values (avoid pulling WiFi.h into host checks;
// main.cpp static_asserts they match).
enum WifiAuthKind : int {
  kAuthOpen = 0,
  kAuthWep = 1,
  kAuthWpa = 2,
  kAuthWpa2 = 3,
  kAuthWpaWpa2 = 4,
  kAuthWpa2Enterprise = 5,
  kAuthWpa3 = 6,
  kAuthWpa2Wpa3 = 7,
  kAuthWapi = 8,
  kAuthWpa3Ent192 = 9,
};

// Risk-coded palette as 0xRRGGBB (full channel; scale with rssiToLevel later).
inline uint32_t encryptionToColor(int authMode) {
  switch (authMode) {
    case kAuthOpen:
      return 0xFF0000;  // red — open
    case kAuthWep:
      return 0xFF6600;  // orange
    case kAuthWpa:
      return 0xFFCC00;  // yellow
    case kAuthWpa2:
    case kAuthWpaWpa2:
    case kAuthWpa2Enterprise:
      return 0x00FF00;  // green
    case kAuthWpa3:
    case kAuthWpa2Wpa3:
    case kAuthWpa3Ent192:
      return 0x00CCFF;  // cyan/blue
    default:
      return 0x880088;  // dim magenta — unknown
  }
}

// Map RSSI (-100..-30 dBm) to a per-channel level 100..255. The floor keeps weak
// APs visible after the strip's global MAX_BRIGHTNESS cap.
inline uint8_t rssiToLevel(int rssi) {
  constexpr int kRssiMin = -100;
  constexpr int kRssiMax = -30;
  constexpr int kLevelMin = 100;
  constexpr int kLevelMax = 255;
  if (rssi <= kRssiMin) return kLevelMin;
  if (rssi >= kRssiMax) return kLevelMax;
  return static_cast<uint8_t>(kLevelMin + (rssi - kRssiMin) * (kLevelMax - kLevelMin) /
                                              (kRssiMax - kRssiMin));
}

// Scale each channel of 0xRRGGBB by level/255 (level 0..255).
inline uint32_t scaleRgb(uint32_t rgb, uint8_t level) {
  const uint32_t r = ((rgb >> 16) & 0xFF) * level / 255;
  const uint32_t g = ((rgb >> 8) & 0xFF) * level / 255;
  const uint32_t b = (rgb & 0xFF) * level / 255;
  return (r << 16) | (g << 8) | b;
}
