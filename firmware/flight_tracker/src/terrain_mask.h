#pragma once

#include <stdint.h>

namespace terrain {

enum class Cell : uint8_t { Water = 0, Land = 1 };

void update(float lat, float lon, float radiusNm);
Cell cell(uint16_t x, uint8_t y);
bool ready();

} // namespace terrain
