#pragma once

#include "adsb_client.h"

#include <tinker/runtime_context.h>

namespace card {

void draw(tinker::RuntimeContext &runtime, const adsb::Aircraft &aircraft,
          uint8_t index, uint8_t count, uint8_t speedUnit,
          uint8_t distanceUnit, uint8_t brightness);

} // namespace card
