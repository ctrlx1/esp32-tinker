#include "justin_display.h"

#include "boot_network_display.h"
#include "programs/program.h"

void JustinDisplay::begin() {
  Display.begin();
  Display.setIntensity(0);
  Display.setTextAlignment(PA_CENTER);
}

void JustinDisplay::setBrightness(uint8_t brightness) {
  Display.setIntensity(brightness);
}

void JustinDisplay::showVersion(const char *version) {
  showBootVersion(Display, version);
}

void JustinDisplay::showIp(const IPAddress &address) {
  showBootIpAddress(Display, address);
}

void JustinDisplay::showMessage(const char *message) { Display.print(message); }

void JustinDisplay::beginSetup(const char *apSsid) {
  snprintf(setupScrollBuffer_, sizeof(setupScrollBuffer_),
           "Connect to hotspot %s", apSsid);
  Display.displayClear();
  Display.displayScroll(setupScrollBuffer_, PA_LEFT, PA_SCROLL_LEFT, 80);
}

void JustinDisplay::tickSetup() {
  if (Display.displayAnimate()) {
    Display.displayReset();
  }
}
