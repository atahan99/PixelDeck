#pragma once

#include <stdint.h>

#ifndef MAX_BRIGHTNESS
#define MAX_BRIGHTNESS 20
#endif

// Arduino ESP32 wifi_auth_mode_t values (avoid pulling WiFi.h into host checks).
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
};

// Risk-coded palette as 0xRRGGBB (full channel; scale with brightness later).
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
      return 0x00CCFF;  // cyan/blue
    default:
      return 0x880088;  // dim magenta — unknown
  }
}

// Map RSSI (~-100..-30 dBm) into a safe LED brightness band.
inline uint8_t rssiToBrightness(int rssi) {
  constexpr int kRssiMin = -100;
  constexpr int kRssiMax = -30;
  constexpr int kBriMin = 4;
  constexpr int kBriMax = MAX_BRIGHTNESS;

  if (rssi <= kRssiMin) {
    return static_cast<uint8_t>(kBriMin);
  }
  if (rssi >= kRssiMax) {
    return static_cast<uint8_t>(kBriMax);
  }

  const int span = kRssiMax - kRssiMin;
  const int bri =
      kBriMin + ((rssi - kRssiMin) * (kBriMax - kBriMin) + span / 2) / span;
  if (bri < kBriMin) {
    return static_cast<uint8_t>(kBriMin);
  }
  if (bri > kBriMax) {
    return static_cast<uint8_t>(kBriMax);
  }
  return static_cast<uint8_t>(bri);
}

inline uint32_t scaleRgb(uint32_t rgb, uint8_t bri) {
  // bri is 0..MAX_BRIGHTNESS; keep LEDs well under full drive.
  const uint8_t r = static_cast<uint8_t>(((rgb >> 16) & 0xFF) * bri / 255);
  const uint8_t g = static_cast<uint8_t>(((rgb >> 8) & 0xFF) * bri / 255);
  const uint8_t b = static_cast<uint8_t>((rgb & 0xFF) * bri / 255);
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

inline uint32_t styleForNetwork(int authMode, int rssi) {
  return scaleRgb(encryptionToColor(authMode), rssiToBrightness(rssi));
}
