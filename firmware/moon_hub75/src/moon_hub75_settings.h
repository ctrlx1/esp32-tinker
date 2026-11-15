#pragma once

#include <stdint.h>

struct MoonHub75Settings {
  uint16_t scrollSpeedMs = 75;
  uint8_t brightness = 4;
};

constexpr uint16_t MOON_HUB75_MIN_SCROLL_SPEED_MS = 1;
constexpr uint16_t MOON_HUB75_MAX_SCROLL_SPEED_MS = 10000;
