#include "max7219_display.h"

#include <Arduino.h>
#include <cstdio>

namespace tinker {
namespace {

MD_MAX72XX::moduleType_t moduleType(Max7219ModuleType type) {
  switch (type) {
  case Max7219ModuleType::Generic:
    return MD_MAX72XX::GENERIC_HW;
  case Max7219ModuleType::Fc16:
    return MD_MAX72XX::FC16_HW;
  case Max7219ModuleType::Parola:
    return MD_MAX72XX::PAROLA_HW;
  case Max7219ModuleType::IcStation:
    return MD_MAX72XX::ICSTATION_HW;
  default:
    return MD_MAX72XX::GENERIC_HW;
  }
}

textPosition_t textPosition(TextAlignment alignment) {
  switch (alignment) {
  case TextAlignment::Center:
    return PA_CENTER;
  case TextAlignment::Right:
    return PA_RIGHT;
  case TextAlignment::Left:
  default:
    return PA_LEFT;
  }
}

} // namespace

const DisplayCapabilities Max7219Display::kCapabilities = {
    {&Max7219Display::clearText, &Max7219Display::print,
     &Max7219Display::startScroll, &Max7219Display::animate,
     &Max7219Display::resetAnimation, &Max7219Display::buffer,
     &Max7219Display::bufferSize},
    {&Max7219Display::setBrightness},
    {&Max7219Display::showVersion, &Max7219Display::showIp,
     &Max7219Display::beginSetup, &Max7219Display::tickSetup},
    {&Max7219Display::width, &Max7219Display::height,
     &Max7219Display::beginFrame, &Max7219Display::endFrame,
     &Max7219Display::clearFrame, &Max7219Display::setPoint},
};

Max7219Display::Max7219Display(const Config &config)
    : display_(moduleType(config.moduleType), config.chipSelectPin,
               config.moduleCount) {}

void Max7219Display::begin() {
  display_.begin();
  display_.setIntensity(0);
  display_.setTextAlignment(PA_CENTER);
}

RuntimeContext Max7219Display::runtimeContext() {
  return RuntimeContext(this, &kCapabilities);
}

MD_Parola &Max7219Display::parola(void *context) {
  return static_cast<Max7219Display *>(context)->display_;
}

MD_MAX72XX &Max7219Display::matrix(void *context) {
  return *parola(context).getGraphicObject();
}

void Max7219Display::clearText(void *context) {
  parola(context).displayClear();
}

void Max7219Display::print(void *context, const char *message) {
  parola(context).print(message);
}

void Max7219Display::startScroll(void *context, const char *text,
                                 TextAlignment alignment, uint16_t speedMs) {
  textPosition_t position = textPosition(alignment);
  parola(context).setTextAlignment(position);
  parola(context).displayScroll(text, position, PA_SCROLL_LEFT, speedMs);
}

bool Max7219Display::animate(void *context) {
  return parola(context).displayAnimate();
}

void Max7219Display::resetAnimation(void *context) {
  parola(context).displayReset();
}

char *Max7219Display::buffer(void *context) {
  return static_cast<Max7219Display *>(context)->textBuffer_;
}

size_t Max7219Display::bufferSize(void *) { return kTextBufferSize; }

void Max7219Display::setBrightness(void *context, uint8_t brightness) {
  parola(context).setIntensity(brightness);
}

void Max7219Display::showVersion(void *context, const char *version) {
  MD_Parola &display = parola(context);
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print(version);
  delay(700);
  display.displayClear();
}

void Max7219Display::showIp(void *context, const IPAddress &address) {
  Max7219Display &adapter = *static_cast<Max7219Display *>(context);
  address.toString().toCharArray(adapter.textBuffer_, kTextBufferSize);

  adapter.display_.displayClear();
  adapter.display_.displayScroll(adapter.textBuffer_, PA_LEFT, PA_SCROLL_LEFT,
                                 80);
  while (!adapter.display_.displayAnimate()) {
    delay(10);
  }
  delay(2000);
}

void Max7219Display::beginSetup(void *context, const char *apSsid) {
  Max7219Display &adapter = *static_cast<Max7219Display *>(context);
  snprintf(adapter.textBuffer_, kTextBufferSize, "Connect to hotspot %s",
           apSsid);
  adapter.display_.displayClear();
  adapter.display_.displayScroll(adapter.textBuffer_, PA_LEFT, PA_SCROLL_LEFT,
                                 80);
}

void Max7219Display::tickSetup(void *context) {
  if (parola(context).displayAnimate()) {
    parola(context).displayReset();
  }
}

uint16_t Max7219Display::width(void *context) {
  return matrix(context).getColumnCount();
}

uint8_t Max7219Display::height(void *) { return 8; }

void Max7219Display::beginFrame(void *context) {
  matrix(context).control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
}

void Max7219Display::endFrame(void *context) {
  matrix(context).control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix(context).update();
}

void Max7219Display::clearFrame(void *context) { matrix(context).clear(); }

void Max7219Display::setPoint(void *context, uint8_t row, uint16_t column,
                              bool on) {
  matrix(context).setPoint(row, column, on);
}

} // namespace tinker
