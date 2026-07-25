// Host self-check for wifi_style mapping. Run: c++ -std=c++17 -I src -DMAX_BRIGHTNESS=20 test/test_wifi_style.cpp -o /tmp/wifi_style_check && /tmp/wifi_style_check
#include "wifi_style.h"

#include <cassert>
#include <cstdio>

int main() {
  assert(encryptionToColor(kAuthOpen) == 0xFF0000u);
  assert(encryptionToColor(kAuthWep) == 0xFF6600u);
  assert(encryptionToColor(kAuthWpa) == 0xFFCC00u);
  assert(encryptionToColor(kAuthWpa2) == 0x00FF00u);
  assert(encryptionToColor(kAuthWpa3) == 0x00CCFFu);
  assert(encryptionToColor(99) == 0x880088u);

  assert(rssiToBrightness(-100) == 4);
  assert(rssiToBrightness(-30) == MAX_BRIGHTNESS);
  assert(rssiToBrightness(-120) == 4);
  assert(rssiToBrightness(-10) == MAX_BRIGHTNESS);

  const uint8_t mid = rssiToBrightness(-65);
  assert(mid > 4 && mid < MAX_BRIGHTNESS);

  const uint32_t styled = styleForNetwork(kAuthOpen, -30);
  assert(((styled >> 16) & 0xFF) > 0);
  assert((styled & 0xFF) == 0);

  const uint32_t weak = styleForNetwork(kAuthOpen, -100);
  assert(((weak >> 16) & 0xFF) < ((styled >> 16) & 0xFF));

  std::puts("wifi_style self-check OK");
  return 0;
}
