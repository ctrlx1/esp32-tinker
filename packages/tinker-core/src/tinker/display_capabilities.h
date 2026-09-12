#pragma once

#include <IPAddress.h>
#include <stdint.h>

namespace tinker {

struct TextDisplayCapabilities {
  void *context;
  void (*showMessage)(void *context, const char *message);
};

struct BrightnessDisplayCapabilities {
  void *context;
  void (*setBrightness)(void *context, uint8_t brightness);
};

struct BootDisplayCapabilities {
  void *context;
  void (*showVersion)(void *context, const char *version);
  void (*showIp)(void *context, const IPAddress &address);
};

struct FramebufferDisplayCapabilities {
  void *context;
  uint16_t (*width)(void *context);
  uint8_t (*height)(void *context);
  void (*beginFrame)(void *context);
  void (*endFrame)(void *context);
  void (*clear)(void *context);
  void (*setPoint)(void *context, uint8_t row, uint16_t column, bool on);

  bool valid() const {
    return context && width && height && beginFrame && endFrame && clear &&
           setPoint;
  }
};

} // namespace tinker
