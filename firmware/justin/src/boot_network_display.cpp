#include "boot_network_display.h"

namespace {
const uint16_t BOOT_VERSION_HOLD_MS = 700;
const uint16_t BOOT_IP_SCROLL_SPEED_MS = 80;
const uint16_t BOOT_IP_HOLD_MS = 2000;
} // namespace

void showBootVersion(MD_Parola &display, const char *version) {
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print(version);
  delay(BOOT_VERSION_HOLD_MS);
  display.displayClear();
}

void showBootIpAddress(MD_Parola &display, const IPAddress &ipAddress) {
  char bootIpBuffer[16];
  ipAddress.toString().toCharArray(bootIpBuffer, sizeof(bootIpBuffer));

  display.displayClear();
  display.displayScroll(bootIpBuffer, PA_LEFT, PA_SCROLL_LEFT,
                        BOOT_IP_SCROLL_SPEED_MS);
  while (!display.displayAnimate()) {
    delay(10);
  }
  delay(BOOT_IP_HOLD_MS);
}
