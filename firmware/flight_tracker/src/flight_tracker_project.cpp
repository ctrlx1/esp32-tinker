#include "flight_tracker_project.h"

#include "app_version.h"
#include "setup_html.h"

#include <stdlib.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr uint8_t kDefaultBrightness = 4;
#ifdef WOKWI_SIM
constexpr unsigned long kFrameIntervalMs = 80;
#else
constexpr unsigned long kFrameIntervalMs = 16;
#endif
constexpr const char *kHelloWorld = "Hello\nWorld";

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

bool parseLongInRange(String value, long minimum, long maximum, long &result) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }

  char *end = nullptr;
  long parsed = strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0' || parsed < minimum ||
      parsed > maximum) {
    return false;
  }

  result = parsed;
  return true;
}

void hsvToRgb(uint16_t hue, uint8_t &red, uint8_t &green, uint8_t &blue) {
  const uint8_t region = static_cast<uint8_t>(hue / 60);
  const uint8_t remainder =
      static_cast<uint8_t>(((hue % 60) * 255) / 60);
  const uint8_t rising = remainder;
  const uint8_t falling = static_cast<uint8_t>(255 - remainder);

  switch (region) {
  case 0:
    red = 255;
    green = rising;
    blue = 0;
    break;
  case 1:
    red = falling;
    green = 255;
    blue = 0;
    break;
  case 2:
    red = 0;
    green = 255;
    blue = rising;
    break;
  case 3:
    red = 0;
    green = falling;
    blue = 255;
    break;
  case 4:
    red = rising;
    green = 0;
    blue = 255;
    break;
  default:
    red = 255;
    green = 0;
    blue = falling;
    break;
  }
}

} // namespace

FlightTrackerProject::FlightTrackerProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition FlightTrackerProject::definition() const {
  return {"flight_tracker",
          "Flight Tracker",
          APP_VERSION,
          "FlightTrk-",
          {"flight_tracker", "cfgVer", kSettingsVersion}};
}

bool FlightTrackerProject::migrateSettings(Preferences &,
                                           uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void FlightTrackerProject::loadSettings(Preferences &preferences) {
  settings_.brightness =
      preferences.getUChar("brightness", kDefaultBrightness);
  if (settings_.brightness > 15) {
    settings_.brightness = kDefaultBrightness;
  }

  Serial.print(" flight_tracker brightness=");
  Serial.println(settings_.brightness);
}

void FlightTrackerProject::saveSettings(Preferences &preferences) const {
  preferences.putUChar("brightness", settings_.brightness);
}

uint8_t FlightTrackerProject::displayBrightness() const {
  return settings_.brightness;
}

String FlightTrackerProject::buildPortalPage(const String &ip,
                                             const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool FlightTrackerProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("brightness")) {
    error = "Missing flight tracker settings.";
    return false;
  }

  long brightness;
  if (!parseLongInRange(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be between 0 and 15.";
    return false;
  }

  settings_.brightness = static_cast<uint8_t>(brightness);
  return true;
}

void FlightTrackerProject::drawHelloWorld() {
  if (!runtime_.hasText() || !runtime_.hasColor()) {
    Serial.println("Flight tracker display is missing text or color.");
    started_ = false;
    return;
  }

  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  hsvToRgb(hue_, red, green, blue);
  runtime_.setBrightness(settings_.brightness);
  runtime_.setTextColor(red, green, blue);
  runtime_.clearText();
  runtime_.showMessage(kHelloWorld);
  started_ = true;
}

void FlightTrackerProject::startPrograms() {
  hue_ = 0;
  lastFrameMs_ = 0;
  drawHelloWorld();
}

void FlightTrackerProject::tickPrograms() {
  const unsigned long now = millis();
  if (started_ && lastFrameMs_ != 0 &&
      now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;
  hue_ = static_cast<uint16_t>((hue_ + 1) % 360);
  drawHelloWorld();
}
