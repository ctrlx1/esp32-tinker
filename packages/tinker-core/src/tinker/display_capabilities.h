#pragma once

#include <IPAddress.h>
#include <stddef.h>
#include <stdint.h>

namespace tinker {

enum class TextAlignment : uint8_t {
  Left,
  Center,
  Right,
};

struct TextDisplayCapabilities {
  void (*clear)(void *context);
  void (*print)(void *context, const char *message);
  void (*startScroll)(void *context, const char *text,
                      TextAlignment alignment, uint16_t speedMs);
  bool (*animate)(void *context);
  void (*resetAnimation)(void *context);
  char *(*buffer)(void *context);
  size_t (*bufferSize)(void *context);
};

struct BrightnessDisplayCapabilities {
  void (*setBrightness)(void *context, uint8_t brightness);
};

struct BootDisplayCapabilities {
  void (*showVersion)(void *context, const char *version);
  void (*showIp)(void *context, const IPAddress &address);
  void (*beginSetup)(void *context, const char *apSsid);
  void (*tickSetup)(void *context);
};

struct FramebufferDisplayCapabilities {
  uint16_t (*width)(void *context);
  uint8_t (*height)(void *context);
  void (*beginFrame)(void *context);
  void (*endFrame)(void *context);
  void (*clear)(void *context);
  void (*setPoint)(void *context, uint8_t row, uint16_t column, bool on);
};

struct ColorDisplayCapabilities {
  void (*setTextColor)(void *context, uint8_t r, uint8_t g, uint8_t b);
  void (*fill)(void *context, uint8_t r, uint8_t g, uint8_t b);
  void (*setPixel)(void *context, uint8_t row, uint16_t column, uint8_t r,
                   uint8_t g, uint8_t b);
  void (*beginFrame)(void *context);
  void (*endFrame)(void *context);
  void (*blitRgb565)(void *context, const uint16_t *pixels, uint16_t width,
                     uint8_t height);
};

struct DisplayCapabilities {
  TextDisplayCapabilities text;
  BrightnessDisplayCapabilities brightness;
  BootDisplayCapabilities boot;
  FramebufferDisplayCapabilities framebuffer;
  ColorDisplayCapabilities color;
};

} // namespace tinker
