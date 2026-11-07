// Host self-check for wifi_style mapping.
// Run: c++ -std=c++17 -Wall -Wextra -I src test/test_wifi_style.cpp -o /tmp/wifi_style_check && /tmp/wifi_style_check
#include "wifi_style.h"

#include <cassert>
#include <cstdio>

int main() {
  assert(encryptionToColor(kAuthOpen) == 0xFF0000u);
  assert(encryptionToColor(kAuthWep) == 0xFF6600u);
  assert(encryptionToColor(kAuthWpa) == 0xFFCC00u);
  assert(encryptionToColor(kAuthWpa2) == 0x00FF00u);
  assert(encryptionToColor(kAuthWpa2Enterprise) == 0x00FF00u);
  assert(encryptionToColor(kAuthWpa3) == 0x00CCFFu);
  assert(encryptionToColor(kAuthWpa3Ent192) == 0x00CCFFu);
  assert(encryptionToColor(kAuthWapi) == 0x880088u);
  assert(encryptionToColor(99) == 0x880088u);

  assert(rssiToLevel(-120) == 100);
  assert(rssiToLevel(-100) == 100);
  assert(rssiToLevel(-30) == 255);
  assert(rssiToLevel(-10) == 255);
  const uint8_t mid = rssiToLevel(-65);
  assert(mid > 100 && mid < 255);
  assert(rssiToLevel(-50) > rssiToLevel(-80));

  assert(scaleRgb(0xFF0000, 255) == 0xFF0000u);
  assert(scaleRgb(0xFF0000, 0) == 0u);
  const uint32_t strong = scaleRgb(encryptionToColor(kAuthOpen), rssiToLevel(-30));
  const uint32_t weak = scaleRgb(encryptionToColor(kAuthOpen), rssiToLevel(-100));
  assert((strong & 0xFFFF) == 0 && (weak & 0xFFFF) == 0);
  assert((weak >> 16) > 0 && (weak >> 16) < (strong >> 16));

  std::puts("wifi_style self-check OK");
  return 0;
}
