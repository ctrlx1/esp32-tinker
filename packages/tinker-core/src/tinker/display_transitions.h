#pragma once

#include <Arduino.h>
#include <esp_system.h>

#include "display_capabilities.h"

namespace tinker {

class DisplayTransitions {
public:
  void startRandom() {
    if (!seeded_) {
      randomSeed(esp_random());
      seeded_ = true;
    }
    active_ = true;
    kind_ = static_cast<Kind>(random(3));
    lastFrameMs_ = 0;
    step_ = 0;
  }

  bool tick(const FramebufferDisplayCapabilities &display) {
    if (!active_) {
      return true;
    }
    if (!display.valid()) {
      active_ = false;
      return true;
    }

    unsigned long now = millis();
    if (lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameMs) {
      return false;
    }
    lastFrameMs_ = now;

    bool done = false;
    switch (kind_) {
    case Kind::SparkleDissolve:
      done = tickSparkleDissolve(display);
      break;
    case Kind::CurtainClose:
      done = tickCurtainClose(display);
      break;
    case Kind::ColumnWipe:
    default:
      done = tickColumnWipe(display);
      break;
    }
    if (done) {
      active_ = false;
    }
    return done;
  }

private:
  enum class Kind : uint8_t {
    ColumnWipe,
    SparkleDissolve,
    CurtainClose,
  };

  static constexpr unsigned long kFrameMs = 35UL;
  static constexpr uint8_t kSparkleFrames = 32;
  static constexpr uint8_t kSparklePixelsPerFrame = 8;

  static void beginFrame(const FramebufferDisplayCapabilities &display) {
    display.beginFrame(display.context);
  }

  static void endFrame(const FramebufferDisplayCapabilities &display) {
    display.endFrame(display.context);
  }

  static void setColumn(const FramebufferDisplayCapabilities &display,
                        uint16_t column, bool on) {
    if (column >= display.width(display.context)) {
      return;
    }
    uint8_t height = display.height(display.context);
    for (uint8_t row = 0; row < height; row++) {
      display.setPoint(display.context, row, column, on);
    }
  }

  static void clearDisplay(const FramebufferDisplayCapabilities &display) {
    beginFrame(display);
    display.clear(display.context);
    endFrame(display);
  }

  bool tickColumnWipe(const FramebufferDisplayCapabilities &display) {
    uint16_t width = display.width(display.context);
    if (step_ >= width) {
      clearDisplay(display);
      return true;
    }
    beginFrame(display);
    setColumn(display, step_, false);
    step_++;
    endFrame(display);
    return false;
  }

  bool tickSparkleDissolve(const FramebufferDisplayCapabilities &display) {
    uint16_t width = display.width(display.context);
    uint8_t height = display.height(display.context);
    if (step_ >= kSparkleFrames || width == 0 || height == 0) {
      clearDisplay(display);
      return true;
    }
    beginFrame(display);
    for (uint8_t i = 0; i < kSparklePixelsPerFrame; i++) {
      display.setPoint(display.context, random(height), random(width), false);
    }
    step_++;
    endFrame(display);
    return false;
  }

  bool tickCurtainClose(const FramebufferDisplayCapabilities &display) {
    uint16_t width = display.width(display.context);
    uint16_t halfSteps = (width + 1) / 2;
    if (step_ >= halfSteps) {
      clearDisplay(display);
      return true;
    }
    uint16_t left = step_;
    uint16_t right = width - 1 - step_;
    beginFrame(display);
    setColumn(display, left, false);
    setColumn(display, right, false);
    step_++;
    endFrame(display);
    return false;
  }

  bool active_ = false;
  bool seeded_ = false;
  Kind kind_ = Kind::ColumnWipe;
  unsigned long lastFrameMs_ = 0;
  uint16_t step_ = 0;
};

} // namespace tinker
