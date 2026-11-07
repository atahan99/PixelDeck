#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "wifi_style.h"

#ifndef MAX_BRIGHTNESS
#define MAX_BRIGHTNESS 20
#endif
// M5Stack: Atom Matrix LEDs overheat the case above ~20/255.
static_assert(MAX_BRIGHTNESS > 0 && MAX_BRIGHTNESS <= 20, "Atom Matrix brightness cap");

// wifi_style.h mirrors these so it can be tested on the host without WiFi.h.
#define PIXELDECK_SAME_AUTH(ours, idf) static_assert(int(ours) == int(idf), #ours " != " #idf)
PIXELDECK_SAME_AUTH(kAuthOpen, WIFI_AUTH_OPEN);
PIXELDECK_SAME_AUTH(kAuthWep, WIFI_AUTH_WEP);
PIXELDECK_SAME_AUTH(kAuthWpa, WIFI_AUTH_WPA_PSK);
PIXELDECK_SAME_AUTH(kAuthWpa2, WIFI_AUTH_WPA2_PSK);
PIXELDECK_SAME_AUTH(kAuthWpaWpa2, WIFI_AUTH_WPA_WPA2_PSK);
PIXELDECK_SAME_AUTH(kAuthWpa2Enterprise, WIFI_AUTH_WPA2_ENTERPRISE);
PIXELDECK_SAME_AUTH(kAuthWpa3, WIFI_AUTH_WPA3_PSK);
PIXELDECK_SAME_AUTH(kAuthWpa2Wpa3, WIFI_AUTH_WPA2_WPA3_PSK);
PIXELDECK_SAME_AUTH(kAuthWapi, WIFI_AUTH_WAPI_PSK);
PIXELDECK_SAME_AUTH(kAuthWpa3Ent192, WIFI_AUTH_WPA3_ENT_192);
#undef PIXELDECK_SAME_AUTH

namespace {

constexpr int kLedCount = 25;
constexpr int kLedPin = 27;
constexpr int kMatrixW = 5;
constexpr int kMatrixH = 5;
constexpr uint32_t kWifiRescanMs = 4000;  // gap after a scan finishes
constexpr uint32_t kRainbowFrameMs = 40;
constexpr uint32_t kAnimateIntervalMs = 200;
constexpr uint32_t kDigitHoldMs = 600;
constexpr uint32_t kDigitGapMs = 150;
constexpr uint32_t kTiltPollMs = 100;
constexpr float kTiltThresholdG = 0.6f;
constexpr float kTiltMarginG = 0.3f;  // dead zone around diagonals, stops flicker

// Official Atom Matrix RGB path: Adafruit NeoPixel on GPIO27 (NEO_GRB).
// M5.Led RMT is a no-op on Arduino-ESP32 2.0.x / ESP-IDF 4.x.
Adafruit_NeoPixel gStrip(kLedCount, kLedPin, NEO_GRB + NEO_KHZ800);
Preferences gPrefs;

enum class AppMode : uint8_t { Default = 0, Static = 1, Animated = 2, Wifi = 3 };
constexpr uint8_t kModeCount = 4;

AppMode gMode = AppMode::Default;
uint32_t gLastRainbowMs = 0;
uint32_t gLastAnimateMs = 0;
uint32_t gLastTiltMs = 0;
uint16_t gRainbowHue = 0;
uint8_t gRotation = 0;  // quarter turns clockwise
bool gImuOk = false;

bool gScanRunning = false;
uint32_t gNextScanMs = 0;
int gApCount = -1;  // total APs in the last finished scan, -1 = none yet

// Logical (unrotated) frame of the current mode; showMatrix() rotates it out.
uint32_t gFrame[kLedCount] = {};

// 5x5 bitmasks for digits 0..9 (MSB = top-left, row-major).
constexpr uint32_t kDigitGlyph[] = {
    0b0111001010010100101001110u,  // 0
    0b0010001100001000010001110u,  // 1
    0b0111000010011100100001110u,  // 2
    0b0111000010001100001001110u,  // 3
    0b0101001010011100001000010u,  // 4
    0b0111001000011100001001110u,  // 5
    0b0111001000011100101001110u,  // 6
    0b0111000010000100010000100u,  // 7
    0b0111001010011100101001110u,  // 8
    0b0111001010011100001001110u,  // 9
};
constexpr uint8_t kBlankGlyph = 10;

// Overlay: a short glyph sequence drawn over the mode, each step until a deadline.
uint8_t gOverlaySeq[3];
uint8_t gOverlayLen = 0;
uint8_t gOverlayPos = 0;
uint32_t gOverlayStepEndMs = 0;

int rotatedIndex(int i) {
  const int x = i % kMatrixW;
  const int y = i / kMatrixW;
  switch (gRotation) {
    case 1:
      return x * kMatrixW + (kMatrixW - 1 - y);
    case 2:
      return (kMatrixH - 1 - y) * kMatrixW + (kMatrixW - 1 - x);
    case 3:
      return (kMatrixH - 1 - x) * kMatrixW + y;
    default:
      return i;
  }
}

bool overlayActive() { return gOverlayPos < gOverlayLen; }

void showMatrix() {
  for (int i = 0; i < kLedCount; ++i) {
    uint32_t c = gFrame[i];
    if (overlayActive()) {
      const uint8_t g = gOverlaySeq[gOverlayPos];
      const bool lit = g != kBlankGlyph && (kDigitGlyph[g] & (1u << (kLedCount - 1 - i)));
      c = lit ? 0xFFFFFF : 0;
    }
    gStrip.setPixelColor(rotatedIndex(i), c);
  }
  gStrip.show();
}

void setLed(int index, uint32_t rgb888) {
  if (index >= 0 && index < kLedCount) gFrame[index] = rgb888;
}

void setLedXY(int x, int y, uint32_t rgb888) { setLed(y * kMatrixW + x, rgb888); }

void clearFrame() { std::fill(gFrame, gFrame + kLedCount, 0u); }

void startOverlay(const uint8_t* seq, uint8_t len, uint32_t now) {
  std::copy(seq, seq + len, gOverlaySeq);
  gOverlayLen = len;
  gOverlayPos = 0;
  gOverlayStepEndMs = now + kDigitHoldMs;
  showMatrix();
}

void tickOverlay(uint32_t now) {
  if (!overlayActive() || static_cast<int32_t>(now - gOverlayStepEndMs) < 0) return;
  ++gOverlayPos;
  if (overlayActive()) {
    const bool blank = gOverlaySeq[gOverlayPos] == kBlankGlyph;
    gOverlayStepEndMs = now + (blank ? kDigitGapMs : kDigitHoldMs);
  }
  showMatrix();
}

void showNumber(int n, uint32_t now) {
  n = std::max(0, std::min(n, 99));
  if (n < 10) {
    const uint8_t seq[] = {static_cast<uint8_t>(n)};
    startOverlay(seq, 1, now);
  } else {
    const uint8_t seq[] = {static_cast<uint8_t>(n / 10), kBlankGlyph,
                           static_cast<uint8_t>(n % 10)};
    startOverlay(seq, 3, now);
  }
}

uint32_t randomColor() { return gStrip.gamma32(gStrip.ColorHSV(esp_random() & 0xFFFF)); }

void drawRainbowFrame() {
  for (int i = 0; i < kLedCount; ++i) {
    setLed(i, gStrip.gamma32(gStrip.ColorHSV(gRainbowHue + i * (65536 / kLedCount))));
  }
  showMatrix();
  gRainbowHue += 1092;  // ~6 degrees per frame
}

void drawRandomPattern() {
  clearFrame();

  const bool multi = (esp_random() & 1) != 0;
  const uint32_t c1 = randomColor();
  const uint32_t c2 = randomColor();

  switch (esp_random() % 4) {
    case 0: {
      const int n = 5 + static_cast<int>(esp_random() % 12);
      for (int i = 0; i < n; ++i) {
        setLedXY(esp_random() % kMatrixW, esp_random() % kMatrixH,
                 multi && (i & 1) ? c2 : c1);
      }
      break;
    }
    case 1:
      for (int y = 0; y < kMatrixH; ++y) {
        for (int x = 0; x < kMatrixW; ++x) {
          setLedXY(x, y, multi && ((x + y) & 1) ? c2 : c1);
        }
      }
      break;
    case 2:
      for (int x = 0; x < kMatrixW; ++x) {
        setLedXY(x, 0, c1);
        setLedXY(x, kMatrixH - 1, c1);
      }
      for (int y = 1; y < kMatrixH - 1; ++y) {
        setLedXY(0, y, c1);
        setLedXY(kMatrixW - 1, y, c1);
      }
      if (multi) setLedXY(2, 2, c2);
      break;
    default:
      for (int y = 0; y < kMatrixH; ++y) {
        for (int x = 0; x < kMatrixW; ++x) {
          if ((x + y) % 3 == 0) {
            setLedXY(x, y, c1);
          } else if (multi) {
            setLedXY(x, y, c2);
          }
        }
      }
      break;
  }
  showMatrix();
}

struct ApHit {
  int8_t rssi;
  uint8_t enc;
};

// n = WiFi.scanComplete() result: >= 0 APs found, < 0 failed.
void drawWifiResult(int n) {
  clearFrame();
  if (n <= 0) {
    setLedXY(2, 2, 0xFF0000);
    showMatrix();
    if (n == 0) gApCount = 0;
    return;
  }
  gApCount = n;

  std::vector<ApHit> hits(n);
  for (int i = 0; i < n; ++i) {
    hits[i] = {static_cast<int8_t>(WiFi.RSSI(i)), static_cast<uint8_t>(WiFi.encryptionType(i))};
  }
  // Scan results are not ordered; keep the strongest kLedCount.
  const int shown = std::min(n, kLedCount);
  std::partial_sort(hits.begin(), hits.begin() + shown, hits.end(),
                    [](const ApHit& a, const ApHit& b) { return a.rssi > b.rssi; });
  for (int i = 0; i < shown; ++i) {
    setLed(i, scaleRgb(encryptionToColor(hits[i].enc), rssiToLevel(hits[i].rssi)));
  }
  showMatrix();
}

void startScan(uint32_t now) {
  if (WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true) == WIFI_SCAN_FAILED) {
    drawWifiResult(WIFI_SCAN_FAILED);
    gNextScanMs = now + kWifiRescanMs;
    return;
  }
  gScanRunning = true;
}

void tickWifi(uint32_t now) {
  if (gScanRunning) {
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    gScanRunning = false;
    drawWifiResult(n);
    WiFi.scanDelete();
    gNextScanMs = now + kWifiRescanMs;
    Serial.printf("pixeldeck: scan aps=%d\n", n);
    return;
  }
  // A scan issued before STA_START takes ~7 s and trips Arduino's 6 s scan timeout.
  if (!(WiFi.getStatusBits() & STA_STARTED_BIT)) return;
  if (static_cast<int32_t>(now - gNextScanMs) >= 0) startScan(now);
}

void wifiOn(uint32_t now) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/true);  // scan only, never join
  gApCount = -1;
  gNextScanMs = now;  // tickWifi starts it once the STA interface is up
}

void wifiOff() {
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  gScanRunning = false;
}

void tickTilt(uint32_t now) {
  if (!gImuOk || now - gLastTiltMs < kTiltPollMs) return;
  gLastTiltMs = now;
  float ax = 0, ay = 0, az = 0;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;

  // Flat (gravity mostly on z) or near a diagonal: keep the last rotation.
  const float x = std::fabs(ax);
  const float y = std::fabs(ay);
  uint8_t rot = gRotation;
  if (x > kTiltThresholdG && x - y > kTiltMarginG) {
    rot = ax > 0 ? 1 : 3;
  } else if (y > kTiltThresholdG && y - x > kTiltMarginG) {
    rot = ay > 0 ? 2 : 0;
  }
  if (rot != gRotation) {
    gRotation = rot;
    showMatrix();
    Serial.printf("pixeldeck: rotation=%u (ax=%.2f ay=%.2f)\n", rot, ax, ay);
  }
}

uint8_t modeNumber(AppMode mode) { return static_cast<uint8_t>(mode) + 1; }

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

AppMode nextMode(AppMode mode) {
  return static_cast<AppMode>((static_cast<uint8_t>(mode) + 1) % kModeCount);
}

void enterMode(AppMode mode, uint32_t now) {
  if (gMode == AppMode::Wifi && mode != AppMode::Wifi) wifiOff();
  gMode = mode;
  gPrefs.putUChar("mode", static_cast<uint8_t>(mode));

  // The mode starts drawing underneath; the digit overlay hides it until it ends.
  clearFrame();
  showNumber(modeNumber(mode), now);

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
      wifiOn(now);
      break;
  }
  Serial.printf("pixeldeck: mode=%u %s\n", modeNumber(mode), modeName(mode));
}

void initMatrixLeds() {
  gStrip.begin();
  gStrip.setBrightness(MAX_BRIGHTNESS);
  std::fill(gFrame, gFrame + kLedCount, 0xFF0000u);  // boot probe: all red
  showMatrix();
  delay(400);
  clearFrame();
  showMatrix();
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;  // begun below with the Matrix axis fix
  M5.begin(cfg);

  Serial.println();
  Serial.printf("pixeldeck: board=%d\n", static_cast<int>(M5.getBoard()));
  Serial.printf("pixeldeck: NeoPixel pin=%d n=%d bri=%d\n", kLedPin, kLedCount,
                MAX_BRIGHTNESS);

  // Atom Matrix often autodetects as Atom Lite (same I2C pins), which skips the
  // Matrix IMU axis remap, so name the board explicitly.
  gImuOk = M5.Imu.begin(&M5.In_I2C, m5::board_t::board_M5AtomMatrix);
  Serial.printf("pixeldeck: imu=%s\n", gImuOk ? "ok" : "none (no auto-rotate)");

  initMatrixLeds();
  WiFi.mode(WIFI_OFF);

  gPrefs.begin("pixeldeck", /*readOnly=*/false);
  const uint8_t saved = gPrefs.getUChar("mode", 0);
  enterMode(static_cast<AppMode>(saved < kModeCount ? saved : 0), millis());
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  if (M5.BtnA.wasClicked()) {
    enterMode(nextMode(gMode), now);
    return;
  }
  if (gMode == AppMode::Wifi && M5.BtnA.wasHold() && gApCount >= 0) {
    showNumber(gApCount, now);
    Serial.printf("pixeldeck: showing ap count=%d\n", gApCount);
  }

  tickOverlay(now);
  tickTilt(now);

  switch (gMode) {
    case AppMode::Default:
      if (now - gLastRainbowMs >= kRainbowFrameMs) {
        drawRainbowFrame();
        gLastRainbowMs = now;
      }
      break;
    case AppMode::Static:
      break;
    case AppMode::Animated:
      if (now - gLastAnimateMs >= kAnimateIntervalMs) {
        drawRandomPattern();
        gLastAnimateMs = now;
      }
      break;
    case AppMode::Wifi:
      tickWifi(now);
      break;
  }
}
