#include "hub75_display.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace tinker {

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
                               (b >> 3));
}

const DisplayCapabilities Hub75Display::kCapabilities = {
    {&Hub75Display::clearText, &Hub75Display::print, &Hub75Display::startScroll,
     &Hub75Display::animate, &Hub75Display::resetAnimation,
     &Hub75Display::buffer, &Hub75Display::bufferSize},
    {&Hub75Display::setBrightness},
    {&Hub75Display::showVersion, &Hub75Display::showIp,
     &Hub75Display::beginSetup, &Hub75Display::tickSetup},
    {},
    {&Hub75Display::setTextColor, &Hub75Display::fill, &Hub75Display::setPixel,
     &Hub75Display::beginColorFrame, &Hub75Display::endColorFrame,
     &Hub75Display::blitRgb565},
};

#ifndef WOKWI_SIM
HUB75_I2S_CFG Hub75Display::makeMxConfig(const Config &config) {
  const HUB75_I2S_CFG::i2s_pins pins = {
      config.r1, config.g1, config.b1, config.r2,  config.g2,
      config.b2, config.a,  config.b,  config.c,   config.d,
      config.e,  config.lat, config.oe, config.clk};
  HUB75_I2S_CFG mxconfig(config.width, config.height, 1, pins);
  mxconfig.double_buff = true;
  mxconfig.driver = config.driver == Hub75Driver::Fm6124
                        ? HUB75_I2S_CFG::FM6124
                        : HUB75_I2S_CFG::SHIFTREG;
  return mxconfig;
}
#endif

#ifdef WOKWI_SIM
Hub75Display::Hub75Display(const Config &config)
    : pins_(config), canvas_(config.width, config.height), width_(config.width),
      height_(config.height) {}
#else
Hub75Display::Hub75Display(const Config &config)
    : mxconfig_(makeMxConfig(config)), panel_(mxconfig_), width_(config.width),
      height_(config.height) {}
#endif

void Hub75Display::begin() {
#ifdef WOKWI_SIM
  Serial.println(
      "HUB75 Wokwi: GPIO bit-bang (ESP32 I2S LCD/DMA is not emulated)");
  configureSimPins();
  if (prevFrame_ == nullptr && width_ > 0 && height_ > 0) {
    prevFrame_ = new uint16_t[static_cast<size_t>(width_) * height_];
  }
  canvas_.setTextWrap(false);
  canvas_.setTextSize(1);
  applyTextColor();
  const uint16_t startup[][3] = {
      {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
  const char *names[] = {"red", "green", "blue", "white"};
  for (uint8_t i = 0; i < 4; ++i) {
    Serial.print("HUB75 Wokwi startup fill: ");
    Serial.println(names[i]);
    canvas_.fillScreen(rgb565(startup[i][0], startup[i][1], startup[i][2]));
    present();
    delay(400);
  }
#else
  const bool initialized = panel_.begin();
  Serial.print("HUB75 DMA initialization: ");
  Serial.println(initialized ? "ok" : "failed");

  if (!initialized) {
    return;
  }

  panel_.setBrightness8(0);
  panel_.setTextWrap(false);
  panel_.setTextSize(1);
  applyTextColor();
  panel_.clearScreen();
#endif
}

RuntimeContext Hub75Display::runtimeContext() {
  return RuntimeContext(this, &kCapabilities);
}

Hub75Display &Hub75Display::self(void *context) {
  return *static_cast<Hub75Display *>(context);
}

Adafruit_GFX &Hub75Display::gfx() {
#ifdef WOKWI_SIM
  return canvas_;
#else
  return panel_;
#endif
}

void Hub75Display::clearScreen() {
#ifdef WOKWI_SIM
  canvas_.fillScreen(0);
  present();
#else
  panel_.clearScreen();
#endif
}

void Hub75Display::present() {
#ifdef WOKWI_SIM
  flushSim();
#else
  panel_.flipDMABuffer();
#endif
}

void Hub75Display::presentCleared() {
  clearScreen();
#ifndef WOKWI_SIM
  present();
#endif
}

void Hub75Display::presentIfIdle() {
  if (!frameOpen_) {
    present();
  }
}

void Hub75Display::applyTextColor() {
  gfx().setTextColor(rgb565(textR_, textG_, textB_));
}

#ifdef WOKWI_SIM
void Hub75Display::configureSimPins() {
  const int8_t pins[] = {pins_.r1,  pins_.g1, pins_.b1, pins_.r2, pins_.g2,
                         pins_.b2,  pins_.a,  pins_.b,  pins_.c,  pins_.d,
                         pins_.lat, pins_.oe, pins_.clk, pins_.e};
  for (int8_t pin : pins) {
    if (pin >= 0) {
      pinMode(static_cast<uint8_t>(pin), OUTPUT);
      digitalWrite(static_cast<uint8_t>(pin), LOW);
    }
  }
  writePin(pins_.oe, true);
}

void Hub75Display::writePin(int8_t pin, bool high) const {
  if (pin < 0) {
    return;
  }
  digitalWrite(static_cast<uint8_t>(pin), high ? HIGH : LOW);
}

void Hub75Display::pulsePin(int8_t pin) const {
  writePin(pin, true);
  writePin(pin, false);
}

void Hub75Display::flushSim() {
  if (width_ == 0 || height_ < 2) {
    return;
  }

  const uint16_t *pixels = canvas_.getBuffer();
  const uint8_t scanRows = static_cast<uint8_t>(height_ / 2);
  const uint16_t scale = brightness8_ + 1;
  const uint32_t rowBytes = static_cast<uint32_t>(width_) * sizeof(uint16_t);
  const uint8_t shift = static_cast<uint8_t>(8 - kWokwiColorBits);

  writePin(pins_.clk, false);
  writePin(pins_.lat, false);
  writePin(pins_.oe, true);

  uint8_t lastColors = 0xFF;
  for (uint8_t row = 0; row < scanRows; ++row) {
    const uint16_t topOffset = static_cast<uint16_t>(row) * width_;
    const uint16_t botOffset =
        static_cast<uint16_t>(row + scanRows) * width_;
    if (havePrevFrame_ && prevFrame_ != nullptr &&
        memcmp(pixels + topOffset, prevFrame_ + topOffset, rowBytes) == 0 &&
        memcmp(pixels + botOffset, prevFrame_ + botOffset, rowBytes) == 0) {
      continue;
    }

    for (uint16_t column = 0; column < width_; ++column) {
      const uint16_t top = pixels[topOffset + column];
      const uint16_t bottom = pixels[botOffset + column];
      uint8_t topR = static_cast<uint8_t>((((top >> 11) & 0x1F) * 255) / 31);
      uint8_t topG = static_cast<uint8_t>((((top >> 5) & 0x3F) * 255) / 63);
      uint8_t topB = static_cast<uint8_t>(((top & 0x1F) * 255) / 31);
      uint8_t botR =
          static_cast<uint8_t>((((bottom >> 11) & 0x1F) * 255) / 31);
      uint8_t botG =
          static_cast<uint8_t>((((bottom >> 5) & 0x3F) * 255) / 63);
      uint8_t botB = static_cast<uint8_t>(((bottom & 0x1F) * 255) / 31);
      topR = static_cast<uint8_t>((static_cast<uint16_t>(topR) * scale) >> 8);
      topG = static_cast<uint8_t>((static_cast<uint16_t>(topG) * scale) >> 8);
      topB = static_cast<uint8_t>((static_cast<uint16_t>(topB) * scale) >> 8);
      botR = static_cast<uint8_t>((static_cast<uint16_t>(botR) * scale) >> 8);
      botG = static_cast<uint8_t>((static_cast<uint16_t>(botG) * scale) >> 8);
      botB = static_cast<uint8_t>((static_cast<uint16_t>(botB) * scale) >> 8);

      for (uint8_t bit = 0; bit < kWokwiColorBits; ++bit) {
        const uint8_t colors = static_cast<uint8_t>(
            (((topR >> (bit + shift)) & 1) << 5) |
            (((topG >> (bit + shift)) & 1) << 4) |
            (((topB >> (bit + shift)) & 1) << 3) |
            (((botR >> (bit + shift)) & 1) << 2) |
            (((botG >> (bit + shift)) & 1) << 1) |
            ((botB >> (bit + shift)) & 1));
        if (colors != lastColors) {
          writePin(pins_.r1, (colors >> 5) & 1);
          writePin(pins_.g1, (colors >> 4) & 1);
          writePin(pins_.b1, (colors >> 3) & 1);
          writePin(pins_.r2, (colors >> 2) & 1);
          writePin(pins_.g2, (colors >> 1) & 1);
          writePin(pins_.b2, colors & 1);
          lastColors = colors;
        }
        pulsePin(pins_.clk);
      }
    }

    writePin(pins_.a, row & 1);
    writePin(pins_.b, row & 2);
    writePin(pins_.c, row & 4);
    writePin(pins_.d, row & 8);
    pulsePin(pins_.lat);
  }

  if (prevFrame_ != nullptr) {
    memcpy(prevFrame_, pixels,
           static_cast<size_t>(width_) * height_ * sizeof(uint16_t));
    havePrevFrame_ = true;
  }

  writePin(pins_.oe, false);
}
#endif

void Hub75Display::drawCenteredMessage(const char *message) {
  if (!message) {
    return;
  }

  uint8_t lines = 1;
  for (const char *cursor = message; *cursor; ++cursor) {
    if (*cursor == '\n') {
      ++lines;
    }
  }

  const int16_t blockHeight = static_cast<int16_t>(lines) * kGlyphHeight;
  int16_t y = (static_cast<int16_t>(height_) - blockHeight) / 2;
  if (y < 0) {
    y = 0;
  }

  applyTextColor();
  const char *lineStart = message;
  while (*lineStart) {
    const char *lineEnd = lineStart;
    while (*lineEnd && *lineEnd != '\n') {
      ++lineEnd;
    }

    const size_t lineLength = static_cast<size_t>(lineEnd - lineStart);
    const int16_t lineWidth =
        static_cast<int16_t>(lineLength) * kGlyphWidth;
    int16_t x = (static_cast<int16_t>(width_) - lineWidth) / 2;
    if (x < 0) {
      x = 0;
    }

    gfx().setCursor(x, y);
    for (size_t index = 0; index < lineLength; ++index) {
      gfx().write(static_cast<uint8_t>(lineStart[index]));
    }

    y += kGlyphHeight;
    lineStart = *lineEnd ? lineEnd + 1 : lineEnd;
  }
  present();
}

void Hub75Display::drawScrolledMessage() {
  applyTextColor();
  const int16_t y =
      (static_cast<int16_t>(height_) - static_cast<int16_t>(kGlyphHeight)) / 2;
  gfx().setCursor(scrollX_, y < 0 ? 0 : y);
  gfx().print(textBuffer_);
  present();
}

void Hub75Display::clearText(void *context) {
  Hub75Display &adapter = self(context);
  adapter.scrollActive_ = false;
  adapter.presentCleared();
}

void Hub75Display::print(void *context, const char *message) {
  Hub75Display &adapter = self(context);
  adapter.scrollActive_ = false;
  adapter.clearScreen();
  adapter.drawCenteredMessage(message);
}

void Hub75Display::startScroll(void *context, const char *text,
                               TextAlignment, uint16_t speedMs) {
  Hub75Display &adapter = self(context);
  if (!text) {
    adapter.scrollActive_ = false;
    return;
  }

  strncpy(adapter.textBuffer_, text, kTextBufferSize - 1);
  adapter.textBuffer_[kTextBufferSize - 1] = '\0';
  adapter.scrollSpeedMs_ = speedMs == 0 ? 40 : speedMs;
  adapter.scrollX_ = static_cast<int16_t>(adapter.width_);
  adapter.lastScrollMs_ = millis();
  adapter.scrollActive_ = true;
  adapter.clearScreen();
  adapter.drawScrolledMessage();
}

bool Hub75Display::animate(void *context) {
  Hub75Display &adapter = self(context);
  if (!adapter.scrollActive_) {
    return false;
  }

  const unsigned long now = millis();
  if (now - adapter.lastScrollMs_ < adapter.scrollSpeedMs_) {
    return false;
  }
  adapter.lastScrollMs_ = now;
  --adapter.scrollX_;

  adapter.clearScreen();
  adapter.drawScrolledMessage();

  const int16_t textWidth =
      static_cast<int16_t>(strlen(adapter.textBuffer_)) * kGlyphWidth;
  if (adapter.scrollX_ < -textWidth) {
    return true;
  }
  return false;
}

void Hub75Display::resetAnimation(void *context) {
  Hub75Display &adapter = self(context);
  adapter.scrollX_ = static_cast<int16_t>(adapter.width_);
  adapter.lastScrollMs_ = millis();
}

char *Hub75Display::buffer(void *context) { return self(context).textBuffer_; }

size_t Hub75Display::bufferSize(void *) { return kTextBufferSize; }

void Hub75Display::setBrightness(void *context, uint8_t brightness) {
  if (brightness > 15) {
    brightness = 15;
  }
  const uint8_t scaled = static_cast<uint8_t>(brightness * 17);
#ifdef WOKWI_SIM
  Hub75Display &adapter = self(context);
  if (adapter.brightness8_ != scaled) {
    adapter.brightness8_ = scaled;
    adapter.havePrevFrame_ = false;
  }
#else
  self(context).panel_.setBrightness8(scaled);
#endif
}

void Hub75Display::showVersion(void *context, const char *version) {
  Hub75Display &adapter = self(context);
  adapter.scrollActive_ = false;
  adapter.clearScreen();
  adapter.drawCenteredMessage(version);
  delay(700);
  adapter.presentCleared();
}

void Hub75Display::showIp(void *context, const IPAddress &address) {
  Hub75Display &adapter = self(context);
  address.toString().toCharArray(adapter.textBuffer_, kTextBufferSize);
  startScroll(context, adapter.textBuffer_, TextAlignment::Left, 40);
  while (!animate(context)) {
    delay(10);
  }
  delay(2000);
  adapter.scrollActive_ = false;
  adapter.presentCleared();
}

void Hub75Display::beginSetup(void *context, const char *apSsid) {
  Hub75Display &adapter = self(context);
  snprintf(adapter.textBuffer_, kTextBufferSize, "Connect to hotspot %s",
           apSsid ? apSsid : "");
  startScroll(context, adapter.textBuffer_, TextAlignment::Left, 40);
}

void Hub75Display::tickSetup(void *context) {
  if (animate(context)) {
    resetAnimation(context);
  }
}

void Hub75Display::setTextColor(void *context, uint8_t r, uint8_t g,
                                uint8_t b) {
  Hub75Display &adapter = self(context);
  adapter.textR_ = r;
  adapter.textG_ = g;
  adapter.textB_ = b;
  adapter.applyTextColor();
}

void Hub75Display::fill(void *context, uint8_t r, uint8_t g, uint8_t b) {
  Hub75Display &adapter = self(context);
  adapter.gfx().fillScreen(rgb565(r, g, b));
  adapter.presentIfIdle();
}

void Hub75Display::setPixel(void *context, uint8_t row, uint16_t column,
                            uint8_t r, uint8_t g, uint8_t b) {
  Hub75Display &adapter = self(context);
  adapter.gfx().drawPixel(column, row, rgb565(r, g, b));
  adapter.presentIfIdle();
}

void Hub75Display::beginColorFrame(void *context) {
  self(context).frameOpen_ = true;
}

void Hub75Display::endColorFrame(void *context) {
  Hub75Display &adapter = self(context);
  adapter.frameOpen_ = false;
  adapter.present();
}

void Hub75Display::blitRgb565(void *context, const uint16_t *pixels,
                              uint16_t width, uint8_t height) {
  if (!pixels || width == 0 || height == 0) {
    return;
  }
  Hub75Display &adapter = self(context);
  adapter.gfx().drawRGBBitmap(0, 0, const_cast<uint16_t *>(pixels),
                              static_cast<int16_t>(width),
                              static_cast<int16_t>(height));
  adapter.presentIfIdle();
}

} // namespace tinker
