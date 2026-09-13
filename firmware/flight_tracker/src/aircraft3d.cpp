#include "aircraft3d.h"

#include "../hardware/hub75_profile.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace aircraft3d {
namespace {

constexpr uint16_t kWidth = flight_tracker_hardware::kDisplayWidth;
constexpr uint8_t kHeight = flight_tracker_hardware::kDisplayHeight;
constexpr float kTwoPi = 6.2831853f;
constexpr float kPitch = 0.38f;
constexpr float kCameraZ = 3.85f;
constexpr float kFocal = 54.0f;
constexpr float kYawRate = 3.2f;
constexpr float kRotorRate = 14.0f;
constexpr uint16_t kSkyColor =
    static_cast<uint16_t>(((4 & 0xF8) << 8) | ((8 & 0xFC) << 3) | (22 >> 3));

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Box {
  float x0;
  float y0;
  float z0;
  float x1;
  float y1;
  float z1;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  bool rotor;
};

struct Aircraft {
  const Box *boxes;
  uint8_t boxCount;
  Vec3 rotorHub;
};

struct Basis {
  float cy;
  float sy;
  float cp;
  float sp;
  float cr;
  float sr;
  Vec3 hub;
};

int16_t zBuffer[kHeight][kWidth];
uint16_t colorBuffer[kHeight][kWidth];

constexpr Box kAirliner[] = {
    {-0.13f, -0.12f, -1.32f, 0.13f, 0.14f, 1.32f, 214, 218, 230, false},
    {-0.08f, -0.08f, 1.32f, 0.08f, 0.10f, 1.62f, 214, 218, 230, false},
    {-0.09f, 0.10f, 1.02f, 0.09f, 0.20f, 1.36f, 40, 170, 220, false},
    {-1.28f, -0.03f, -0.18f, -0.13f, 0.03f, 0.48f, 28, 82, 180, false},
    {0.13f, -0.03f, -0.18f, 1.28f, 0.03f, 0.48f, 28, 82, 180, false},
    {-0.74f, -0.17f, -0.06f, -0.48f, -0.03f, 0.34f, 68, 70, 78, false},
    {0.48f, -0.17f, -0.06f, 0.74f, -0.03f, 0.34f, 68, 70, 78, false},
    {-0.03f, 0.12f, -1.32f, 0.03f, 0.54f, -0.92f, 205, 42, 48, false},
    {-0.56f, 0.10f, -1.30f, -0.03f, 0.16f, -0.98f, 205, 42, 48, false},
    {0.03f, 0.10f, -1.30f, 0.56f, 0.16f, -0.98f, 205, 42, 48, false},
};

constexpr Box kFighter[] = {
    {-0.11f, -0.09f, -1.12f, 0.11f, 0.09f, 1.18f, 88, 108, 98, false},
    {-0.06f, -0.06f, 1.18f, 0.06f, 0.06f, 1.64f, 226, 188, 46, false},
    {-0.08f, 0.09f, 0.42f, 0.08f, 0.24f, 1.02f, 28, 196, 230, false},
    {-1.12f, -0.02f, -0.88f, -0.11f, 0.04f, 0.52f, 52, 74, 86, false},
    {0.11f, -0.02f, -0.88f, 1.12f, 0.04f, 0.52f, 52, 74, 86, false},
    {-0.30f, 0.08f, -1.12f, -0.18f, 0.44f, -0.72f, 88, 108, 98, false},
    {0.18f, 0.08f, -1.12f, 0.30f, 0.44f, -0.72f, 88, 108, 98, false},
    {-0.20f, -0.08f, 0.18f, -0.11f, 0.06f, 0.74f, 38, 42, 48, false},
    {0.11f, -0.08f, 0.18f, 0.20f, 0.06f, 0.74f, 38, 42, 48, false},
};

constexpr Box kProp[] = {
    {-0.16f, -0.14f, -0.92f, 0.16f, 0.16f, 1.02f, 232, 186, 36, false},
    {-0.12f, -0.10f, 1.02f, 0.12f, 0.12f, 1.26f, 232, 186, 36, false},
    {-0.15f, 0.16f, 0.12f, 0.15f, 0.38f, 0.84f, 70, 168, 214, false},
    {-1.22f, 0.32f, -0.08f, -0.16f, 0.40f, 0.54f, 236, 236, 240, false},
    {0.16f, 0.32f, -0.08f, 1.22f, 0.40f, 0.54f, 236, 236, 240, false},
    {-0.03f, 0.14f, -0.92f, 0.03f, 0.56f, -0.52f, 204, 40, 42, false},
    {-0.46f, 0.12f, -0.92f, 0.46f, 0.18f, -0.66f, 204, 40, 42, false},
    {-0.58f, -0.04f, 1.26f, 0.58f, 0.04f, 1.33f, 56, 42, 32, true},
    {-0.04f, -0.58f, 1.26f, 0.04f, 0.58f, 1.33f, 56, 42, 32, true},
};

constexpr Box kHelicopter[] = {
    {-0.28f, -0.16f, -0.28f, 0.28f, 0.22f, 0.52f, 34, 102, 46, false},
    {-0.22f, 0.00f, 0.52f, 0.22f, 0.22f, 0.84f, 48, 186, 204, false},
    {-0.07f, -0.04f, -1.46f, 0.07f, 0.10f, -0.28f, 34, 102, 46, false},
    {-0.03f, 0.08f, -1.50f, 0.03f, 0.44f, -1.18f, 204, 48, 40, false},
    {-0.32f, -0.28f, -0.18f, -0.24f, -0.20f, 0.48f, 92, 92, 98, false},
    {0.24f, -0.28f, -0.18f, 0.32f, -0.20f, 0.48f, 92, 92, 98, false},
    {-1.18f, 0.36f, -0.07f, 1.18f, 0.42f, 0.07f, 186, 38, 40, true},
    {-0.07f, 0.36f, -1.18f, 0.07f, 0.42f, 1.18f, 168, 34, 36, true},
    {-0.22f, 0.16f, -1.50f, -0.03f, 0.40f, -1.36f, 186, 38, 40, false},
};

constexpr Aircraft kAircraft[] = {
    {kAirliner, static_cast<uint8_t>(sizeof(kAirliner) / sizeof(kAirliner[0])),
     {0.0f, 0.0f, 0.0f}},
    {kFighter, static_cast<uint8_t>(sizeof(kFighter) / sizeof(kFighter[0])),
     {0.0f, 0.0f, 0.0f}},
    {kProp, static_cast<uint8_t>(sizeof(kProp) / sizeof(kProp[0])),
     {0.0f, 0.0f, 1.30f}},
    {kHelicopter,
     static_cast<uint8_t>(sizeof(kHelicopter) / sizeof(kHelicopter[0])),
     {0.0f, 0.39f, 0.0f}},
};

constexpr uint8_t kBoxFaces[12][3] = {
    {0, 1, 2}, {0, 2, 3}, {5, 4, 7}, {5, 7, 6}, {4, 0, 3}, {4, 3, 7},
    {1, 5, 6}, {1, 6, 2}, {3, 2, 6}, {3, 6, 7}, {4, 5, 1}, {4, 1, 0},
};

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
                               (b >> 3));
}

Vec3 transformPoint(const Vec3 &local, const Basis &basis, bool spinning) {
  float x = local.x;
  float y = local.y;
  float z = local.z;
  if (spinning) {
    x -= basis.hub.x;
    z -= basis.hub.z;
    const float nx = x * basis.cr + z * basis.sr;
    const float nz = -x * basis.sr + z * basis.cr;
    x = nx + basis.hub.x;
    z = nz + basis.hub.z;
  }
  const float yawX = x * basis.cy + z * basis.sy;
  const float yawZ = -x * basis.sy + z * basis.cy;
  const float pitchY = y * basis.cp - yawZ * basis.sp;
  const float pitchZ = y * basis.sp + yawZ * basis.cp;
  return {yawX, pitchY, pitchZ + kCameraZ};
}

int edge(int ax, int ay, int bx, int by, int px, int py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void fillTriangle(int x0, int y0, float z0, int x1, int y1, float z1, int x2,
                  int y2, float z2, uint16_t color) {
  int minX = x0 < x1 ? x0 : x1;
  minX = minX < x2 ? minX : x2;
  int maxX = x0 > x1 ? x0 : x1;
  maxX = maxX > x2 ? maxX : x2;
  int minY = y0 < y1 ? y0 : y1;
  minY = minY < y2 ? minY : y2;
  int maxY = y0 > y1 ? y0 : y1;
  maxY = maxY > y2 ? maxY : y2;

  if (maxX < 0 || maxY < 0 || minX >= static_cast<int>(kWidth) ||
      minY >= static_cast<int>(kHeight)) {
    return;
  }
  if (minX < 0) {
    minX = 0;
  }
  if (minY < 0) {
    minY = 0;
  }
  if (maxX >= static_cast<int>(kWidth)) {
    maxX = static_cast<int>(kWidth) - 1;
  }
  if (maxY >= static_cast<int>(kHeight)) {
    maxY = static_cast<int>(kHeight) - 1;
  }

  const int area = edge(x0, y0, x1, y1, x2, y2);
  if (area == 0) {
    return;
  }

  const int dw0x = y2 - y1;
  const int dw0y = x1 - x2;
  const int dw1x = y0 - y2;
  const int dw1y = x2 - x0;
  const int dw2x = y1 - y0;
  const int dw2y = x0 - x1;
  int w0Row = edge(x1, y1, x2, y2, minX, minY);
  int w1Row = edge(x2, y2, x0, y0, minX, minY);
  int w2Row = edge(x0, y0, x1, y1, minX, minY);
  const float invArea = 1.0f / static_cast<float>(area);

  for (int y = minY; y <= maxY; ++y) {
    int w0 = w0Row;
    int w1 = w1Row;
    int w2 = w2Row;
    for (int x = minX; x <= maxX; ++x) {
      const bool inside = area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                   : (w0 <= 0 && w1 <= 0 && w2 <= 0);
      if (inside) {
        const float z =
            (static_cast<float>(w0) * z0 + static_cast<float>(w1) * z1 +
             static_cast<float>(w2) * z2) *
            invArea;
        if (z > 0.2f) {
          int16_t encoded = static_cast<int16_t>(z * 256.0f);
          if (encoded < 1) {
            encoded = 1;
          }
          if (encoded < zBuffer[y][x]) {
            zBuffer[y][x] = encoded;
            colorBuffer[y][x] = color;
          }
        }
      }
      w0 += dw0x;
      w1 += dw1x;
      w2 += dw2x;
    }
    w0Row += dw0y;
    w1Row += dw1y;
    w2Row += dw2y;
  }
}

void drawBox(const Box &box, const Basis &basis) {
  const Vec3 corners[8] = {
      {box.x0, box.y0, box.z0}, {box.x1, box.y0, box.z0},
      {box.x1, box.y1, box.z0}, {box.x0, box.y1, box.z0},
      {box.x0, box.y0, box.z1}, {box.x1, box.y0, box.z1},
      {box.x1, box.y1, box.z1}, {box.x0, box.y1, box.z1},
  };

  Vec3 cam[8];
  int sx[8];
  int sy[8];
  for (uint8_t i = 0; i < 8; ++i) {
    cam[i] = transformPoint(corners[i], basis, box.rotor);
    if (cam[i].z < 0.2f) {
      cam[i].z = 0.2f;
    }
    sx[i] = static_cast<int>(static_cast<float>(kWidth) * 0.5f +
                             (cam[i].x / cam[i].z) * kFocal);
    sy[i] = static_cast<int>(static_cast<float>(kHeight) * 0.5f -
                             (cam[i].y / cam[i].z) * kFocal);
  }

  const uint16_t color = rgb565(box.r, box.g, box.b);
  for (uint8_t f = 0; f < 12; ++f) {
    const uint8_t i0 = kBoxFaces[f][0];
    const uint8_t i1 = kBoxFaces[f][1];
    const uint8_t i2 = kBoxFaces[f][2];
    fillTriangle(sx[i0], sy[i0], cam[i0].z, sx[i1], sy[i1], cam[i1].z, sx[i2],
                 sy[i2], cam[i2].z, color);
  }
}

void presentColorBuffer(tinker::RuntimeContext &runtime) {
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

void reset(uint8_t &aircraft, float &yaw, float &rotor) {
  aircraft = 0;
  yaw = 0.0f;
  rotor = 0.0f;
}

void advance(uint8_t &aircraft, float &yaw, float &rotor, unsigned long dtMs) {
  const float dt = static_cast<float>(dtMs) * 0.001f;
  yaw += kYawRate * dt;
  rotor += kRotorRate * dt;
  if (rotor >= kTwoPi) {
    rotor -= kTwoPi;
  }
  if (yaw >= kTwoPi) {
    yaw = 0.0f;
    aircraft = static_cast<uint8_t>((aircraft + 1) % kAircraftCount);
  }
}

void draw(tinker::RuntimeContext &runtime, uint8_t aircraft, float yaw,
          float rotor) {
  if (aircraft >= kAircraftCount) {
    aircraft = 0;
  }

  memset(zBuffer, 0x7F, sizeof(zBuffer));
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      colorBuffer[row][col] = kSkyColor;
    }
  }

  const Aircraft &model = kAircraft[aircraft];
  const Basis basis = {cosf(yaw),      sinf(yaw),      cosf(kPitch),
                       sinf(kPitch),   cosf(rotor),    sinf(rotor),
                       model.rotorHub};
  for (uint8_t i = 0; i < model.boxCount; ++i) {
    drawBox(model.boxes[i], basis);
  }
  presentColorBuffer(runtime);
}

} // namespace aircraft3d
