#pragma once

#include "../hardware/hub75_profile.h"

#include <math.h>

namespace map_geometry {

inline float cornerRadiusPx() {
  const float halfW =
      static_cast<float>(flight_tracker_hardware::kDisplayWidth) * 0.5f;
  const float halfH =
      static_cast<float>(flight_tracker_hardware::kDisplayHeight) * 0.5f;
  return hypotf(halfW, halfH);
}

} // namespace map_geometry
