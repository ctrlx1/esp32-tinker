#pragma once

#include "display_capabilities.h"

namespace tinker {

struct RuntimeContext {
  TextDisplayCapabilities *text = nullptr;
  BrightnessDisplayCapabilities *brightness = nullptr;
  BootDisplayCapabilities *boot = nullptr;
  FramebufferDisplayCapabilities *framebuffer = nullptr;
};

} // namespace tinker
