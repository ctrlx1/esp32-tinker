#include "terrain_mask.h"

#include "land_polygons_data.h"

#include "../hardware/hub75_profile.h"

#include <cmath>
#include <math.h>
#include <string.h>

namespace terrain {
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

Cell mask_[kHeight][kWidth];
bool ready_ = false;
float lastLat_ = 9999.0f;
float lastLon_ = 9999.0f;
float lastRadiusNm_ = -1.0f;

float wrapLon(float lon) {
  while (lon > 180.0f) {
    lon -= 360.0f;
  }
  while (lon < -180.0f) {
    lon += 360.0f;
  }
  return lon;
}

float relLon(float lon, float origin) {
  float delta = lon - origin;
  while (delta > 180.0f) {
    delta -= 360.0f;
  }
  while (delta < -180.0f) {
    delta += 360.0f;
  }
  return delta;
}

bool bboxHits(const land_polygons::Ring &ring, float latCenti, float lonOrigin) {
  if (latCenti < static_cast<float>(ring.minLat) ||
      latCenti > static_cast<float>(ring.maxLat)) {
    return false;
  }
  const float minRel = relLon(static_cast<float>(ring.minLon) * 0.01f, lonOrigin);
  const float maxRel = relLon(static_cast<float>(ring.maxLon) * 0.01f, lonOrigin);
  if (static_cast<int>(ring.maxLon) - static_cast<int>(ring.minLon) > 18000) {
    return true;
  }
  return minRel <= 0.0f && maxRel >= 0.0f;
}

bool ringContains(const land_polygons::Ring &ring, float lat, float lon) {
  const uint16_t count = ring.count;
  if (count < 3) {
    return false;
  }

  bool inside = false;
  float lat0 = static_cast<float>(land_polygons::kVerts[ring.start].lat) * 0.01f;
  float lon0 = relLon(
      static_cast<float>(land_polygons::kVerts[ring.start].lon) * 0.01f, lon);
  for (uint16_t i = 1; i <= count; ++i) {
    const land_polygons::Vert &vertex =
        land_polygons::kVerts[ring.start + (i == count ? 0 : i)];
    const float lat1 = static_cast<float>(vertex.lat) * 0.01f;
    const float lon1 = relLon(static_cast<float>(vertex.lon) * 0.01f, lon);
    if ((lat0 > lat) != (lat1 > lat)) {
      const float cross =
          lon0 + (lon1 - lon0) * (lat - lat0) / (lat1 - lat0);
      if (cross > 0.0f) {
        inside = !inside;
      }
    }
    lat0 = lat1;
    lon0 = lon1;
  }
  return inside;
}

bool classifyLand(float lat, float lon) {
  const float latCenti = lat * 100.0f;
  bool land = false;
  uint16_t landGroup = 0xFFFF;
  for (uint16_t i = 0; i < land_polygons::kRingCount; ++i) {
    const land_polygons::Ring &ring = land_polygons::kRings[i];
    if ((ring.flags & land_polygons::kFlagWater) != 0) {
      continue;
    }
    if ((ring.flags & land_polygons::kFlagHole) != 0) {
      continue;
    }
    if (!bboxHits(ring, latCenti, lon)) {
      continue;
    }
    if (ringContains(ring, lat, lon)) {
      land = true;
      landGroup = ring.group;
      break;
    }
  }
  if (land) {
    for (uint16_t i = 0; i < land_polygons::kRingCount; ++i) {
      const land_polygons::Ring &ring = land_polygons::kRings[i];
      if (ring.group != landGroup ||
          (ring.flags & land_polygons::kFlagHole) == 0) {
        continue;
      }
      if (ringContains(ring, lat, lon)) {
        land = false;
        break;
      }
    }
  }
  for (uint16_t i = 0; i < land_polygons::kRingCount; ++i) {
    const land_polygons::Ring &ring = land_polygons::kRings[i];
    if ((ring.flags & land_polygons::kFlagWater) == 0 ||
        (ring.flags & land_polygons::kFlagHole) != 0) {
      continue;
    }
    if (!bboxHits(ring, latCenti, lon)) {
      continue;
    }
    if (ringContains(ring, lat, lon)) {
      land = false;
      break;
    }
  }
  return land;
}

void rebuild(float lat, float lon, float radiusNm) {
  const float pxPerNm = kRadiusPx / radiusNm;
  memset(mask_, 0, sizeof(mask_));
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      const float dx = (static_cast<float>(col) + 0.5f) - kCenterX;
      const float dy = (static_cast<float>(row) + 0.5f) - kCenterY;
      const float eastNm = dx / pxPerNm;
      const float northNm = -dy / pxPerNm;
      const float sampleLat = lat + northNm / 60.0f;
      const float sampleLon =
          wrapLon(lon + eastNm / (60.0f * cosf(lat * kDegToRad)));
      if (sampleLat < -90.0f || sampleLat > 90.0f) {
        continue;
      }
      mask_[row][col] =
          classifyLand(sampleLat, sampleLon) ? Cell::Land : Cell::Water;
    }
  }
  ready_ = true;
}

} // namespace

void update(float lat, float lon, float radiusNm) {
  if (!(radiusNm > 0.0f) || !std::isfinite(lat) || !std::isfinite(lon)) {
    return;
  }
  if (ready_ && lat == lastLat_ && lon == lastLon_ &&
      radiusNm == lastRadiusNm_) {
    return;
  }
  lastLat_ = lat;
  lastLon_ = lon;
  lastRadiusNm_ = radiusNm;
  rebuild(lat, lon, radiusNm);
}

Cell cell(uint16_t x, uint8_t y) {
  if (x >= kWidth || y >= kHeight) {
    return Cell::Water;
  }
  return mask_[y][x];
}

bool ready() { return ready_; }

} // namespace terrain
