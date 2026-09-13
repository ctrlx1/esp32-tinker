#pragma once

#include "adsb_client.h"

#include <tinker/runtime_context.h>

namespace radar {

void draw(tinker::RuntimeContext &runtime, const adsb::TrackedAircraft *tracks,
          uint8_t count, float radiusNm, float lat, float lon,
          uint8_t brightness);

} // namespace radar
