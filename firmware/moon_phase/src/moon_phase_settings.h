#pragma once

#include <stdint.h>

struct MoonPhaseSettings {
  uint16_t scrollSpeedMs = 75;
  uint8_t brightness = 0;
};

constexpr uint16_t MOON_PHASE_MIN_SCROLL_SPEED_MS = 1;
constexpr uint16_t MOON_PHASE_MAX_SCROLL_SPEED_MS = 10000;
