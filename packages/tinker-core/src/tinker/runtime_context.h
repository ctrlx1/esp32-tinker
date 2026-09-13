#pragma once

#include "display_capabilities.h"

#include <cstring>

namespace tinker {

class RuntimeContext {
public:
  RuntimeContext() : displayContext_(nullptr), displayCapabilities_(nullptr) {}

  RuntimeContext(void *displayContext,
                 const DisplayCapabilities *displayCapabilities)
      : displayContext_(displayContext),
        displayCapabilities_(displayCapabilities) {}

  bool hasText() const {
    return displayCapabilities_ && displayCapabilities_->text.clear &&
           displayCapabilities_->text.print &&
           displayCapabilities_->text.startScroll &&
           displayCapabilities_->text.animate &&
           displayCapabilities_->text.resetAnimation &&
           displayCapabilities_->text.buffer &&
           displayCapabilities_->text.bufferSize;
  }

  bool hasBrightness() const {
    return displayCapabilities_ &&
           displayCapabilities_->brightness.setBrightness;
  }

  bool hasBootDisplay() const {
    return displayCapabilities_ && displayCapabilities_->boot.showVersion &&
           displayCapabilities_->boot.showIp &&
           displayCapabilities_->boot.beginSetup &&
           displayCapabilities_->boot.tickSetup;
  }

  bool hasFramebuffer() const {
    return displayCapabilities_ &&
           displayCapabilities_->framebuffer.width &&
           displayCapabilities_->framebuffer.height &&
           displayCapabilities_->framebuffer.beginFrame &&
           displayCapabilities_->framebuffer.endFrame &&
           displayCapabilities_->framebuffer.clear &&
           displayCapabilities_->framebuffer.setPoint;
  }

  bool hasColor() const {
    return displayCapabilities_ && displayCapabilities_->color.setTextColor &&
           displayCapabilities_->color.fill &&
           displayCapabilities_->color.setPixel;
  }

  void clearText() const {
    if (displayCapabilities_ && displayCapabilities_->text.clear) {
      displayCapabilities_->text.clear(displayContext_);
    }
  }

  void showMessage(const char *message) const {
    if (displayCapabilities_ && displayCapabilities_->text.print) {
      displayCapabilities_->text.print(displayContext_, message);
    }
  }

  void startTextScroll(const char *text, TextAlignment alignment,
                       uint16_t speedMs) const {
    if (displayCapabilities_ && displayCapabilities_->text.startScroll) {
      displayCapabilities_->text.startScroll(displayContext_, text, alignment,
                                             speedMs);
    }
  }

  bool animateText() const {
    return displayCapabilities_ && displayCapabilities_->text.animate
               ? displayCapabilities_->text.animate(displayContext_)
               : false;
  }

  void resetTextAnimation() const {
    if (displayCapabilities_ && displayCapabilities_->text.resetAnimation) {
      displayCapabilities_->text.resetAnimation(displayContext_);
    }
  }

  char *textBuffer() const {
    return displayCapabilities_ && displayCapabilities_->text.buffer
               ? displayCapabilities_->text.buffer(displayContext_)
               : nullptr;
  }

  size_t textBufferSize() const {
    return displayCapabilities_ && displayCapabilities_->text.bufferSize
               ? displayCapabilities_->text.bufferSize(displayContext_)
               : 0;
  }

  bool copyText(const char *text, size_t maxLength = SIZE_MAX) const {
    char *target = textBuffer();
    size_t capacity = textBufferSize();
    if (!target || capacity == 0 || !text) {
      return false;
    }
    size_t copyLength = capacity - 1;
    if (copyLength > maxLength) {
      copyLength = maxLength;
    }
    strncpy(target, text, copyLength);
    target[copyLength] = '\0';
    return true;
  }

  void setBrightness(uint8_t brightness) const {
    if (displayCapabilities_ &&
        displayCapabilities_->brightness.setBrightness) {
      displayCapabilities_->brightness.setBrightness(displayContext_,
                                                     brightness);
    }
  }

  void showBootVersion(const char *version) const {
    if (displayCapabilities_ && displayCapabilities_->boot.showVersion) {
      displayCapabilities_->boot.showVersion(displayContext_, version);
    }
  }

  void showBootIp(const IPAddress &address) const {
    if (displayCapabilities_ && displayCapabilities_->boot.showIp) {
      displayCapabilities_->boot.showIp(displayContext_, address);
    }
  }

  void beginSetup(const char *apSsid) const {
    if (displayCapabilities_ && displayCapabilities_->boot.beginSetup) {
      displayCapabilities_->boot.beginSetup(displayContext_, apSsid);
    }
  }

  void tickSetup() const {
    if (displayCapabilities_ && displayCapabilities_->boot.tickSetup) {
      displayCapabilities_->boot.tickSetup(displayContext_);
    }
  }

  uint16_t displayWidth() const {
    return displayCapabilities_ && displayCapabilities_->framebuffer.width
               ? displayCapabilities_->framebuffer.width(displayContext_)
               : 0;
  }

  uint8_t displayHeight() const {
    return displayCapabilities_ && displayCapabilities_->framebuffer.height
               ? displayCapabilities_->framebuffer.height(displayContext_)
               : 0;
  }

  void beginFrame() const {
    if (displayCapabilities_ &&
        displayCapabilities_->framebuffer.beginFrame) {
      displayCapabilities_->framebuffer.beginFrame(displayContext_);
    }
  }

  void endFrame() const {
    if (displayCapabilities_ && displayCapabilities_->framebuffer.endFrame) {
      displayCapabilities_->framebuffer.endFrame(displayContext_);
    }
  }

  void clearFrame() const {
    if (displayCapabilities_ && displayCapabilities_->framebuffer.clear) {
      displayCapabilities_->framebuffer.clear(displayContext_);
    }
  }

  void setPoint(uint8_t row, uint16_t column, bool on) const {
    if (displayCapabilities_ && displayCapabilities_->framebuffer.setPoint) {
      displayCapabilities_->framebuffer.setPoint(displayContext_, row, column,
                                                 on);
    }
  }

  void setTextColor(uint8_t r, uint8_t g, uint8_t b) const {
    if (displayCapabilities_ && displayCapabilities_->color.setTextColor) {
      displayCapabilities_->color.setTextColor(displayContext_, r, g, b);
    }
  }

  void fillColor(uint8_t r, uint8_t g, uint8_t b) const {
    if (displayCapabilities_ && displayCapabilities_->color.fill) {
      displayCapabilities_->color.fill(displayContext_, r, g, b);
    }
  }

  void setPixelColor(uint8_t row, uint16_t column, uint8_t r, uint8_t g,
                     uint8_t b) const {
    if (displayCapabilities_ && displayCapabilities_->color.setPixel) {
      displayCapabilities_->color.setPixel(displayContext_, row, column, r, g,
                                           b);
    }
  }

private:
  void *displayContext_;
  const DisplayCapabilities *displayCapabilities_;
};

} // namespace tinker
