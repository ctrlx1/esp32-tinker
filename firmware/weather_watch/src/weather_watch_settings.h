#pragma once

#include <stdint.h>

struct WeatherWatchSettings {
  uint8_t brightness = 0;
};

constexpr uint8_t WEATHER_WATCH_DEFAULT_BRIGHTNESS = 0;
constexpr uint8_t WEATHER_WATCH_MAX_BRIGHTNESS = 15;
