#include "radar_view.h"

#include "terrain_mask.h"

#include "../hardware/hub75_profile.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace radar {
namespace {

constexpr uint16_t kWidth = flight_tracker_hardware::kDisplayWidth;
constexpr uint8_t kHeight = flight_tracker_hardware::kDisplayHeight;
constexpr float kCenterX = static_cast<float>(kWidth) * 0.5f;
constexpr float kCenterY = static_cast<float>(kHeight) * 0.5f;
constexpr float kRadiusPx =
    (kWidth > kHeight ? static_cast<float>(kWidth)
                      : static_cast<float>(kHeight)) *
    0.5f;
constexpr float kDegToRad = 0.01745329252f;
constexpr int kSpriteHalfPx = 2;

constexpr uint8_t kBgR = 2;
constexpr uint8_t kBgG = 6;
constexpr uint8_t kBgB = 14;
constexpr uint8_t kCircleR = 0;
constexpr uint8_t kCircleG = 90;
constexpr uint8_t kCircleB = 50;
constexpr uint8_t kObserverR = 180;
constexpr uint8_t kObserverG = 230;
constexpr uint8_t kObserverB = 255;
constexpr uint8_t kPlaneR = 200;
constexpr uint8_t kPlaneG = 230;
constexpr uint8_t kPlaneB = 70;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
                               (b >> 3));
}

uint16_t colorBuffer[kHeight][kWidth];

bool inPanel(int x, int y) {
  return x >= 0 && y >= 0 && x < static_cast<int>(kWidth) &&
         y < static_cast<int>(kHeight);
}

bool inCircle(int x, int y) {
  const float dx = (static_cast<float>(x) + 0.5f) - kCenterX;
  const float dy = (static_cast<float>(y) + 0.5f) - kCenterY;
  return dx * dx + dy * dy <= kRadiusPx * kRadiusPx;
}

void setClipped(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (!inPanel(x, y) || !inCircle(x, y)) {
    return;
  }
  colorBuffer[y][x] = rgb565(r, g, b);
}

void setPanel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (!inPanel(x, y)) {
    return;
  }
  colorBuffer[y][x] = rgb565(r, g, b);
}

void fillBackground() {
  const uint16_t color = rgb565(kBgR, kBgG, kBgB);
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      colorBuffer[row][col] = color;
    }
  }
}

uint8_t scaleChannel(uint8_t value, uint8_t level) {
  const uint16_t scaled = static_cast<uint16_t>(value) * level / 15;
  return scaled == 0 && value > 0 ? 1 : static_cast<uint8_t>(scaled);
}

void drawTerrain(uint8_t brightness) {
  uint8_t level = static_cast<uint8_t>((brightness * 3) / 4);
  if (level < 1) {
    level = 1;
  }
  const uint8_t landR = scaleChannel(24, level);
  const uint8_t landG = scaleChannel(170, level);
  const uint8_t landB = scaleChannel(36, level);
  const uint8_t waterR = scaleChannel(12, level);
  const uint8_t waterG = scaleChannel(48, level);
  const uint8_t waterB = scaleChannel(200, level);

  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      if (terrain::ready() &&
          terrain::cell(col, row) == terrain::Cell::Land) {
        colorBuffer[row][col] = rgb565(landR, landG, landB);
      } else {
        colorBuffer[row][col] = rgb565(waterR, waterG, waterB);
      }
    }
  }
}

void drawCircle() {
  int x = static_cast<int>(lroundf(kRadiusPx));
  int y = 0;
  int err = 1 - x;
  const int cx = static_cast<int>(lroundf(kCenterX));
  const int cy = static_cast<int>(lroundf(kCenterY));

  while (x >= y) {
    setPanel(cx + x, cy + y, kCircleR, kCircleG, kCircleB);
    setPanel(cx + y, cy + x, kCircleR, kCircleG, kCircleB);
    setPanel(cx - y, cy + x, kCircleR, kCircleG, kCircleB);
    setPanel(cx - x, cy + y, kCircleR, kCircleG, kCircleB);
    setPanel(cx - x, cy - y, kCircleR, kCircleG, kCircleB);
    setPanel(cx - y, cy - x, kCircleR, kCircleG, kCircleB);
    setPanel(cx + y, cy - x, kCircleR, kCircleG, kCircleB);
    setPanel(cx + x, cy - y, kCircleR, kCircleG, kCircleB);
    ++y;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      --x;
      err += 2 * (y - x) + 1;
    }
  }
}

void drawObserver() {
  const int cx = static_cast<int>(lroundf(kCenterX));
  const int cy = static_cast<int>(lroundf(kCenterY));
  setClipped(cx, cy, kObserverR, kObserverG, kObserverB);
  setClipped(cx - 1, cy, kObserverR, kObserverG, kObserverB);
  setClipped(cx + 1, cy, kObserverR, kObserverG, kObserverB);
  setClipped(cx, cy - 1, kObserverR, kObserverG, kObserverB);
  setClipped(cx, cy + 1, kObserverR, kObserverG, kObserverB);
}

// Local offsets: +x starboard, +y nose. Track 0 = north.
constexpr int8_t kPlaneLocal[][2] = {
    {0, 2},  {-1, 1}, {1, 1}, {0, 1}, {0, 0}, {-2, -1}, {2, -1},
};

void drawPlane(const adsb::TrackedAircraft &aircraft, float pxPerNm) {
  const float posX = kCenterX + aircraft.eastNm * pxPerNm;
  const float posY = kCenterY - aircraft.northNm * pxPerNm;
  const float trackRad = static_cast<float>(aircraft.trackDeg) * kDegToRad;
  const float cosT = cosf(trackRad);
  const float sinT = sinf(trackRad);

  for (uint8_t i = 0; i < sizeof(kPlaneLocal) / sizeof(kPlaneLocal[0]); ++i) {
    const float lx = static_cast<float>(kPlaneLocal[i][0]);
    const float ly = static_cast<float>(kPlaneLocal[i][1]);
    const float eastPx = lx * cosT + ly * sinT;
    const float northPx = -lx * sinT + ly * cosT;
    const int x = static_cast<int>(lroundf(posX + eastPx));
    const int y = static_cast<int>(lroundf(posY - northPx));
    setClipped(x, y, kPlaneR, kPlaneG, kPlaneB);
  }
}

void present(tinker::RuntimeContext &runtime) {
  if (runtime.hasRgb565Blit()) {
    runtime.beginColorFrame();
    runtime.blitRgb565(&colorBuffer[0][0], kWidth, kHeight);
    runtime.endColorFrame();
    return;
  }

  runtime.beginColorFrame();
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      const uint16_t color = colorBuffer[row][col];
      runtime.setPixelColor(row, col,
                            static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 /
                                                 31),
                            static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 /
                                                 63),
                            static_cast<uint8_t>((color & 0x1F) * 255 / 31));
    }
  }
  runtime.endColorFrame();
}

} // namespace

void draw(tinker::RuntimeContext &runtime, const adsb::TrackedAircraft *tracks,
          uint8_t count, float radiusNm, float lat, float lon,
          uint8_t brightness) {
  if (radiusNm > 0.0f) {
    terrain::update(lat, lon, radiusNm);
  }
  fillBackground();
  drawTerrain(brightness);
  drawCircle();
  drawObserver();

  if (tracks && radiusNm > 0.0f) {
    const float pxPerNm = kRadiusPx / radiusNm;
    const float maxDistNm =
        (kRadiusPx + static_cast<float>(kSpriteHalfPx)) / pxPerNm;
    for (uint8_t i = 0; i < count; ++i) {
      const float distNm =
          sqrtf(tracks[i].eastNm * tracks[i].eastNm +
                tracks[i].northNm * tracks[i].northNm);
      if (distNm > maxDistNm) {
        continue;
      }
      drawPlane(tracks[i], pxPerNm);
    }
  }

  present(runtime);
}

} // namespace radar
