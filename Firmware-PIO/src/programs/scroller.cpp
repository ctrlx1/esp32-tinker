#include "scroller.h"

#include <cstring>

static const char DEFAULT_SCROLL_MESSAGE[] = "ESP32 Tinker";
static char scrollTextBuffer[MAX_SCROLL_MESSAGE_LENGTH + 1];

static void copyScrollMessage(const ProgramConfig &cfg) {
  const char *source = cfg.scrollMessage.length() > 0
                           ? cfg.scrollMessage.c_str()
                           : DEFAULT_SCROLL_MESSAGE;
  strncpy(scrollTextBuffer, source, MAX_SCROLL_MESSAGE_LENGTH);
  scrollTextBuffer[MAX_SCROLL_MESSAGE_LENGTH] = '\0';
}

void scrollerStart(const ProgramConfig &cfg) {
  copyScrollMessage(cfg);
  Display.displayClear();
  Display.setTextAlignment(PA_LEFT);
  Display.displayScroll(scrollTextBuffer, PA_LEFT, PA_SCROLL_LEFT,
                        cfg.scrollSpeedMs);
}

void scrollerTick(const ProgramConfig &cfg) {
  (void)cfg;
  if (Display.displayAnimate()) {
    Display.displayReset();
  }
}
