#pragma once

#include <WString.h>
#include <stddef.h>
#include <stdint.h>

struct RealWeatherSettings {
  String weatherPostalCode;
  uint16_t scrollSpeedMs = 75;
  uint8_t brightness = 0;
};

constexpr uint16_t REAL_WEATHER_MIN_SCROLL_SPEED_MS = 1;
constexpr uint16_t REAL_WEATHER_MAX_SCROLL_SPEED_MS = 10000;
constexpr size_t REAL_WEATHER_MAX_POSTAL_CODE_LENGTH = 10;
