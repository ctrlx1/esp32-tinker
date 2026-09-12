#pragma once

#include <MD_Parola.h>
#include <tinker/runtime_context.h>

namespace tinker {

enum class Max7219ModuleType : uint8_t {
  Generic,
  Fc16,
  Parola,
  IcStation,
};

struct Max7219DisplayConfig {
  Max7219ModuleType moduleType;
  uint8_t chipSelectPin;
  uint8_t moduleCount;
};

class Max7219Display {
public:
  using Config = Max7219DisplayConfig;

  explicit Max7219Display(const Config &config);

  void begin();
  RuntimeContext runtimeContext();

  Max7219Display(const Max7219Display &) = delete;
  Max7219Display &operator=(const Max7219Display &) = delete;

private:
  static MD_Parola &parola(void *context);
  static MD_MAX72XX &matrix(void *context);

  static void clearText(void *context);
  static void print(void *context, const char *message);
  static void startScroll(void *context, const char *text,
                          TextAlignment alignment, uint16_t speedMs);
  static bool animate(void *context);
  static void resetAnimation(void *context);
  static char *buffer(void *context);
  static size_t bufferSize(void *context);

  static void setBrightness(void *context, uint8_t brightness);

  static void showVersion(void *context, const char *version);
  static void showIp(void *context, const IPAddress &address);
  static void beginSetup(void *context, const char *apSsid);
  static void tickSetup(void *context);

  static uint16_t width(void *context);
  static uint8_t height(void *context);
  static void beginFrame(void *context);
  static void endFrame(void *context);
  static void clearFrame(void *context);
  static void setPoint(void *context, uint8_t row, uint16_t column, bool on);

  static const DisplayCapabilities kCapabilities;
  static constexpr size_t kTextBufferSize = 128;

  MD_Parola display_;
  char textBuffer_[kTextBufferSize] = "";
};

} // namespace tinker
