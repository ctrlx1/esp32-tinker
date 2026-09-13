#pragma once

#ifdef WOKWI_SIM
#include <Adafruit_GFX.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif
#include <tinker/runtime_context.h>

namespace tinker {

struct Hub75DisplayConfig {
  uint16_t width;
  uint8_t height;
  int8_t r1;
  int8_t g1;
  int8_t b1;
  int8_t r2;
  int8_t g2;
  int8_t b2;
  int8_t a;
  int8_t b;
  int8_t c;
  int8_t d;
  int8_t e;
  int8_t lat;
  int8_t oe;
  int8_t clk;
};

class Hub75Display {
public:
  using Config = Hub75DisplayConfig;

  explicit Hub75Display(const Config &config);

  void begin();
  RuntimeContext runtimeContext();

  Hub75Display(const Hub75Display &) = delete;
  Hub75Display &operator=(const Hub75Display &) = delete;

private:
  static Hub75Display &self(void *context);
#ifndef WOKWI_SIM
  static HUB75_I2S_CFG makeMxConfig(const Config &config);
#endif

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

  static void setTextColor(void *context, uint8_t r, uint8_t g, uint8_t b);
  static void fill(void *context, uint8_t r, uint8_t g, uint8_t b);
  static void setPixel(void *context, uint8_t row, uint16_t column, uint8_t r,
                       uint8_t g, uint8_t b);

  Adafruit_GFX &gfx();
  void clearScreen();
  void present();
  void applyTextColor();
  void drawCenteredMessage(const char *message);
  void drawScrolledMessage();
#ifdef WOKWI_SIM
  void configureSimPins();
  void writePin(int8_t pin, bool high) const;
  void pulsePin(int8_t pin) const;
  void flushSim();
#endif

  static const DisplayCapabilities kCapabilities;
  static constexpr size_t kTextBufferSize = 384;
  static constexpr uint8_t kGlyphWidth = 6;
  static constexpr uint8_t kGlyphHeight = 8;
#ifdef WOKWI_SIM
  static constexpr uint8_t kWokwiColorBits = 4;
#endif

#ifdef WOKWI_SIM
  Config pins_;
  GFXcanvas16 canvas_;
  uint8_t brightness8_ = 128;
#else
  HUB75_I2S_CFG mxconfig_;
  MatrixPanel_I2S_DMA panel_;
#endif
  uint16_t width_;
  uint8_t height_;
  uint8_t textR_ = 255;
  uint8_t textG_ = 255;
  uint8_t textB_ = 255;
  char textBuffer_[kTextBufferSize] = "";
  int16_t scrollX_ = 0;
  uint16_t scrollSpeedMs_ = 40;
  unsigned long lastScrollMs_ = 0;
  bool scrollActive_ = false;
};

} // namespace tinker
