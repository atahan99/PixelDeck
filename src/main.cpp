#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#include <algorithm>

#include "wifi_style.h"

namespace {

constexpr int kLedCount = 25;
constexpr int kLedPin = 27;
constexpr int kMatrixW = 5;
constexpr int kMatrixH = 5;
constexpr uint32_t kWifiRescanMs = 4000;
constexpr uint32_t kRainbowFrameMs = 40;
constexpr uint32_t kAnimateIntervalMs = 200;
constexpr uint32_t kDigitHoldMs = 700;

// Official Atom Matrix RGB path: Adafruit NeoPixel on GPIO27 (NEO_GRB).
// M5.Led RMT is a no-op on Arduino-ESP32 2.0.x / ESP-IDF 4.x.
Adafruit_NeoPixel gStrip(kLedCount, kLedPin, NEO_GRB + NEO_KHZ800);

enum class AppMode : uint8_t { Default = 0, Static = 1, Animated = 2, Wifi = 3 };

AppMode gMode = AppMode::Default;
uint32_t gLastWifiScanMs = 0;
uint32_t gLastRainbowMs = 0;
uint32_t gLastAnimateMs = 0;
uint16_t gRainbowHue = 0;

// 5x5 bitmasks for digits 1..4 (MSB = top-left, row-major).
constexpr uint32_t kDigitGlyph[] = {
    0,
    0b0010001100001000010001110u,  // 1
    0b0111000010011100100001110u,  // 2
    0b0111000010001100001001110u,  // 3
    0b0101001010011100001000010u,  // 4
};

void setLed(int index, uint32_t rgb888) {
  if (index < 0 || index >= kLedCount) {
    return;
  }
  gStrip.setPixelColor(index, gStrip.Color((rgb888 >> 16) & 0xFF,
                                           (rgb888 >> 8) & 0xFF, rgb888 & 0xFF));
}

void setLedXY(int x, int y, uint32_t rgb888) {
  setLed(y * kMatrixW + x, rgb888);
}

void fillMatrix(uint32_t rgb888) {
  const uint32_t c = gStrip.Color((rgb888 >> 16) & 0xFF, (rgb888 >> 8) & 0xFF,
                                  rgb888 & 0xFF);
  for (int i = 0; i < kLedCount; ++i) {
    gStrip.setPixelColor(i, c);
  }
  gStrip.show();
}

void clearMatrix() {
  gStrip.clear();
  gStrip.show();
}

void showMatrix() { gStrip.show(); }

uint8_t modeNumber(AppMode mode) {
  return static_cast<uint8_t>(mode) + 1;
}

const char* modeName(AppMode mode) {
  switch (mode) {
    case AppMode::Default:
      return "Default";
    case AppMode::Static:
      return "Static";
    case AppMode::Animated:
      return "Animated";
    default:
      return "Wifi";
  }
}

void showModeNumber(uint8_t n) {
  if (n < 1 || n > 4) {
    return;
  }
  gStrip.clear();
  const uint32_t glyph = kDigitGlyph[n];
  for (int i = 0; i < kLedCount; ++i) {
    if (glyph & (1u << (kLedCount - 1 - i))) {
      setLed(i, 0xFFFFFF);
    }
  }
  showMatrix();
  delay(kDigitHoldMs);
  clearMatrix();
}

uint32_t hsvToRgb(uint16_t h, uint8_t s, uint8_t v) {
  h %= 360;
  if (s == 0) {
    return (static_cast<uint32_t>(v) << 16) | (static_cast<uint32_t>(v) << 8) | v;
  }
  const uint8_t region = h / 60;
  const uint8_t rem = (h % 60) * 255 / 60;
  const uint8_t p = static_cast<uint8_t>((v * (255 - s)) / 255);
  const uint8_t q = static_cast<uint8_t>((v * (255 - (s * rem) / 255)) / 255);
  const uint8_t t =
      static_cast<uint8_t>((v * (255 - (s * (255 - rem)) / 255)) / 255);

  uint8_t r = 0, g = 0, b = 0;
  switch (region) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

uint8_t rssiChannelScale(int rssi) {
  const uint8_t bri = rssiToBrightness(rssi);
  return static_cast<uint8_t>(100 + (bri - 4) * 155 / (MAX_BRIGHTNESS - 4));
}

void drawRainbowFrame() {
  for (int i = 0; i < kLedCount; ++i) {
    const uint16_t h = (gRainbowHue + i * (360 / kLedCount)) % 360;
    setLed(i, hsvToRgb(h, 255, 255));
  }
  showMatrix();
  gRainbowHue = (gRainbowHue + 6) % 360;
}

void drawRandomPattern() {
  gStrip.clear();

  const bool multi = (esp_random() & 1) != 0;
  const uint32_t c1 = esp_random() & 0xFFFFFF;
  const uint32_t c2 = esp_random() & 0xFFFFFF;

  const int kind = esp_random() % 4;
  switch (kind) {
    case 0: {
      const int n = 5 + static_cast<int>(esp_random() % 12);
      for (int i = 0; i < n; ++i) {
        setLedXY(esp_random() % kMatrixW, esp_random() % kMatrixH,
                 multi && (i & 1) ? c2 : c1);
      }
      break;
    }
    case 1: {
      for (int y = 0; y < kMatrixH; ++y) {
        for (int x = 0; x < kMatrixW; ++x) {
          setLedXY(x, y, multi && ((x + y) & 1) ? c2 : c1);
        }
      }
      break;
    }
    case 2: {
      for (int x = 0; x < kMatrixW; ++x) {
        setLedXY(x, 0, c1);
        setLedXY(x, kMatrixH - 1, c1);
      }
      for (int y = 1; y < kMatrixH - 1; ++y) {
        setLedXY(0, y, c1);
        setLedXY(kMatrixW - 1, y, c1);
      }
      if (multi) {
        setLedXY(2, 2, c2);
      }
      break;
    }
    default: {
      for (int y = 0; y < kMatrixH; ++y) {
        for (int x = 0; x < kMatrixW; ++x) {
          const uint32_t c = ((x + y) % 3 == 0) ? c1 : (multi ? c2 : 0);
          if (c != 0) {
            setLedXY(x, y, c);
          }
        }
      }
      break;
    }
  }
  showMatrix();
}

struct ApHit {
  int8_t rssi;
  uint8_t enc;
};

void runWifiScanAndDraw() {
  const int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  gStrip.clear();

  if (n <= 0) {
    setLedXY(2, 2, 0xFF0000);
    showMatrix();
    WiFi.scanDelete();
    return;
  }

  ApHit hits[kLedCount];
  const int count = std::min(n, kLedCount);
  for (int i = 0; i < count; ++i) {
    hits[i].rssi = static_cast<int8_t>(WiFi.RSSI(i));
    hits[i].enc = static_cast<uint8_t>(WiFi.encryptionType(i));
  }

  std::sort(hits, hits + count, [](const ApHit& a, const ApHit& b) {
    return a.rssi > b.rssi;
  });

  for (int i = 0; i < count; ++i) {
    setLed(i, scaleRgb(encryptionToColor(hits[i].enc),
                       rssiChannelScale(hits[i].rssi)));
  }
  showMatrix();
  WiFi.scanDelete();
}

AppMode nextMode(AppMode mode) {
  switch (mode) {
    case AppMode::Default:
      return AppMode::Static;
    case AppMode::Static:
      return AppMode::Animated;
    case AppMode::Animated:
      return AppMode::Wifi;
    default:
      return AppMode::Default;
  }
}

void enterMode(AppMode mode) {
  gMode = mode;
  showModeNumber(modeNumber(mode));
  const uint32_t now = millis();

  switch (mode) {
    case AppMode::Default:
      gRainbowHue = 0;
      drawRainbowFrame();
      gLastRainbowMs = now;
      break;
    case AppMode::Static:
      drawRandomPattern();
      break;
    case AppMode::Animated:
      drawRandomPattern();
      gLastAnimateMs = now;
      break;
    case AppMode::Wifi:
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(true, true);
      delay(50);
      runWifiScanAndDraw();
      gLastWifiScanMs = now;
      break;
  }
}

bool initMatrixLeds() {
  pinMode(kLedPin, OUTPUT);
  gStrip.begin();
  gStrip.setBrightness(MAX_BRIGHTNESS);
  gStrip.clear();
  gStrip.show();

  fillMatrix(0xFF0000);
  delay(400);
  clearMatrix();
  return true;
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  Serial.println();
  Serial.printf("atom-matrix: board=%d\n", static_cast<int>(M5.getBoard()));
  Serial.printf("atom-matrix: NeoPixel pin=%d n=%d bri=%d\n", kLedPin, kLedCount,
                MAX_BRIGHTNESS);

  initMatrixLeds();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);

  enterMode(AppMode::Default);
  Serial.printf("atom-matrix: mode=%u %s\n", modeNumber(gMode), modeName(gMode));
}

void loop() {
  M5.update();

  if (M5.BtnA.wasClicked()) {
    enterMode(nextMode(gMode));
    Serial.printf("atom-matrix: mode=%u %s\n", modeNumber(gMode), modeName(gMode));
    return;
  }

  const uint32_t now = millis();

  if (gMode == AppMode::Default) {
    if (now - gLastRainbowMs >= kRainbowFrameMs) {
      drawRainbowFrame();
      gLastRainbowMs = now;
    }
    return;
  }

  if (gMode == AppMode::Static) {
    return;
  }

  if (gMode == AppMode::Animated) {
    if (now - gLastAnimateMs >= kAnimateIntervalMs) {
      drawRandomPattern();
      gLastAnimateMs = now;
    }
    return;
  }

  if (now - gLastWifiScanMs >= kWifiRescanMs) {
    runWifiScanAndDraw();
    gLastWifiScanMs = now;
  }
}
