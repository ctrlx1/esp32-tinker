#pragma once

#include <stdint.h>

struct ScrollColor {
  const char *name;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

// Ice stays first so a missing saved choice keeps the original scroll color.
constexpr ScrollColor kScrollColors[] = {
    {"Ice", 200, 220, 255},     {"Silver", 176, 186, 204},
    {"White", 255, 255, 255},   {"Gold", 255, 196, 64},
    {"Amber", 255, 148, 40},    {"Rose", 255, 120, 156},
    {"Mint", 96, 255, 176},     {"Violet", 176, 128, 255},
    {"Light Blue", 144, 204, 255}, {"Blue", 48, 128, 255},
    {"Dark Blue", 20, 56, 160},
};

constexpr uint8_t kScrollColorCount =
    sizeof(kScrollColors) / sizeof(kScrollColors[0]);

struct MoonHub75Settings {
  uint16_t scrollSpeedMs = 75;
  uint8_t brightness = 4;
  uint8_t textColor = 0;
};

constexpr uint16_t MOON_HUB75_MIN_SCROLL_SPEED_MS = 1;
constexpr uint16_t MOON_HUB75_MAX_SCROLL_SPEED_MS = 10000;
