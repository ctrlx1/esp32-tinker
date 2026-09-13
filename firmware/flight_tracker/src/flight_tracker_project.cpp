#include "flight_tracker_project.h"

#include "aircraft3d.h"
#include "app_version.h"
#include "setup_html.h"

#include <stdlib.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr uint8_t kDefaultBrightness = 4;
constexpr unsigned long kFrameIntervalMs = 16;

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

void FlightTrackerProject::drawAircraft() {
  if (!runtime_.hasColor()) {
    Serial.println("Flight tracker display is missing color.");
    started_ = false;
    return;
  }

  runtime_.setBrightness(settings_.brightness);
  aircraft3d::draw(runtime_, aircraft_, yaw_, rotor_);
  started_ = true;
}

void FlightTrackerProject::startPrograms() {
  aircraft3d::reset(aircraft_, yaw_, rotor_);
  lastFrameMs_ = 0;
  drawAircraft();
}

void FlightTrackerProject::tickPrograms() {
  const unsigned long now = millis();
  if (started_ && lastFrameMs_ != 0 &&
      now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  unsigned long dtMs = 0;
  if (started_ && lastFrameMs_ != 0) {
    dtMs = now - lastFrameMs_;
    if (dtMs > 100) {
      dtMs = 100;
    }
  }
  lastFrameMs_ = now;
  if (started_ && dtMs > 0) {
    aircraft3d::advance(aircraft_, yaw_, rotor_, dtMs);
  }
  drawAircraft();
}
