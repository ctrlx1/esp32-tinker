#pragma once

#include <WString.h>
#include <stddef.h>
#include <stdint.h>

struct StarterSettings {
  String message = "Hello World";
  uint16_t scrollSpeedMs = 75;
  uint8_t brightness = 4;
};

constexpr size_t STARTER_MESSAGE_MAX_LENGTH = 64;
constexpr uint16_t STARTER_MIN_SCROLL_SPEED_MS = 1;
constexpr uint16_t STARTER_MAX_SCROLL_SPEED_MS = 10000;
