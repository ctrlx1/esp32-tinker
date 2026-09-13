#include "scroller.h"

static const char DEFAULT_SCROLL_MESSAGE[] = "ESP32 Tinker";

static char *copyScrollMessage(const ProgramConfig &cfg) {
  tinker::RuntimeContext &runtime = programRuntimeContext();
  const char *source = cfg.scrollMessage.length() > 0
                           ? cfg.scrollMessage.c_str()
                           : DEFAULT_SCROLL_MESSAGE;
  return runtime.copyText(source, MAX_SCROLL_MESSAGE_LENGTH)
             ? runtime.textBuffer()
             : nullptr;
}

void scrollerStart(const ProgramConfig &cfg) {
  tinker::RuntimeContext &runtime = programRuntimeContext();
  char *scrollText = copyScrollMessage(cfg);
  if (!scrollText) {
    return;
  }
  runtime.clearText();
  runtime.startTextScroll(scrollText, tinker::TextAlignment::Left,
                          cfg.scrollSpeedMs);
}

void scrollerTick(const ProgramConfig &cfg) {
  (void)cfg;
  tinker::RuntimeContext &runtime = programRuntimeContext();
  if (runtime.animateText()) {
    runtime.resetTextAnimation();
  }
}
