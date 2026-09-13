#include "flight_tracker_project.h"

#include "adsb_client.h"
#include "app_version.h"
#include "radar_view.h"
#include "setup_html.h"

#include <cmath>
#include <stdlib.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr unsigned long kFrameIntervalMs = 16;

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

bool parseFloatStrict(String value, float minimum, float maximum, float &out) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }
  char *end = nullptr;
  const float parsed = strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0' || !std::isfinite(parsed) ||
      parsed < minimum || parsed > maximum) {
    return false;
  }
  out = parsed;
  return true;
}

bool parseLongStrict(String value, long minimum, long maximum, long &out) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }
  char *end = nullptr;
  const long parsed = strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0' || parsed < minimum ||
      parsed > maximum) {
    return false;
  }
  out = parsed;
  return true;
}

float radiusToNm(float radius, uint8_t unit) {
  constexpr float kMilesToNm = 0.868976f;
  constexpr float kKmToNm = 0.539957f;
  return radius * (unit == FLIGHT_RADIUS_UNIT_KM ? kKmToNm : kMilesToNm);
}

bool validRadius(float radius, uint8_t unit) {
  return std::isfinite(radius) && radius > 0.0f &&
         (unit == FLIGHT_RADIUS_UNIT_MI || unit == FLIGHT_RADIUS_UNIT_KM) &&
         radiusToNm(radius, unit) <= MAX_FLIGHT_RADIUS_NM;
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
  settings_.flightLat = preferences.getFloat("fltLat", DEFAULT_FLIGHT_LAT);
  if (!std::isfinite(settings_.flightLat) || settings_.flightLat < -90.0f ||
      settings_.flightLat > 90.0f) {
    settings_.flightLat = DEFAULT_FLIGHT_LAT;
  }

  settings_.flightLon = preferences.getFloat("fltLon", DEFAULT_FLIGHT_LON);
  if (!std::isfinite(settings_.flightLon) || settings_.flightLon < -180.0f ||
      settings_.flightLon > 180.0f) {
    settings_.flightLon = DEFAULT_FLIGHT_LON;
  }

  settings_.flightRadius =
      preferences.getFloat("fltRad", DEFAULT_FLIGHT_RADIUS);
  settings_.flightRadiusUnit =
      preferences.getUChar("fltRadUnit", DEFAULT_FLIGHT_RADIUS_UNIT);
  if (!validRadius(settings_.flightRadius, settings_.flightRadiusUnit)) {
    settings_.flightRadius = DEFAULT_FLIGHT_RADIUS;
    settings_.flightRadiusUnit = DEFAULT_FLIGHT_RADIUS_UNIT;
  }

  settings_.flightSpeedUnit =
      preferences.getUChar("fltSpdUnit", DEFAULT_FLIGHT_SPEED_UNIT);
  if (settings_.flightSpeedUnit > FLIGHT_SPEED_UNIT_KPH) {
    settings_.flightSpeedUnit = DEFAULT_FLIGHT_SPEED_UNIT;
  }

  settings_.brightness =
      preferences.getUChar("brightness", DEFAULT_BRIGHTNESS);
  if (settings_.brightness > 15) {
    settings_.brightness = DEFAULT_BRIGHTNESS;
  }

  Serial.print(" flight_tracker lat=");
  Serial.print(settings_.flightLat, 5);
  Serial.print(" lon=");
  Serial.print(settings_.flightLon, 5);
  Serial.print(" radius=");
  Serial.print(settings_.flightRadius);
  Serial.print(" brightness=");
  Serial.println(settings_.brightness);
}

void FlightTrackerProject::saveSettings(Preferences &preferences) const {
  preferences.putFloat("fltLat", settings_.flightLat);
  preferences.putFloat("fltLon", settings_.flightLon);
  preferences.putFloat("fltRad", settings_.flightRadius);
  preferences.putUChar("fltRadUnit", settings_.flightRadiusUnit);
  preferences.putUChar("fltSpdUnit", settings_.flightSpeedUnit);
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
  page.replace("LAT_PLACEHOLDER", String(settings_.flightLat, 6));
  page.replace("LON_PLACEHOLDER", String(settings_.flightLon, 6));
  page.replace("RADIUS_PLACEHOLDER", String(settings_.flightRadius, 2));
  page.replace("RADIUS_UNIT_PLACEHOLDER",
               String(settings_.flightRadiusUnit));
  page.replace("SPEED_UNIT_PLACEHOLDER", String(settings_.flightSpeedUnit));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool FlightTrackerProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("lat") || !request.hasArg("lon") ||
      !request.hasArg("radius") || !request.hasArg("radiusUnit") ||
      !request.hasArg("speedUnit") || !request.hasArg("brightness")) {
    error = "Missing flight tracker settings.";
    return false;
  }

  float lat;
  float lon;
  float radius;
  long radiusUnit;
  long speedUnit;
  long brightness;
  if (!parseFloatStrict(request.arg("lat"), -90.0f, 90.0f, lat)) {
    error = "Latitude must be a number from -90 through 90.";
    return false;
  }
  if (!parseFloatStrict(request.arg("lon"), -180.0f, 180.0f, lon)) {
    error = "Longitude must be a number from -180 through 180.";
    return false;
  }
  if (!parseLongStrict(request.arg("radiusUnit"), FLIGHT_RADIUS_UNIT_MI,
                       FLIGHT_RADIUS_UNIT_KM, radiusUnit)) {
    error = "Radius unit is invalid.";
    return false;
  }
  if (!parseFloatStrict(request.arg("radius"), 0.000001f, 1000000.0f,
                        radius) ||
      !validRadius(radius, static_cast<uint8_t>(radiusUnit))) {
    error = "Radius must be positive and no more than 250 nautical miles.";
    return false;
  }
  if (!parseLongStrict(request.arg("speedUnit"), FLIGHT_SPEED_UNIT_KT,
                       FLIGHT_SPEED_UNIT_KPH, speedUnit)) {
    error = "Speed unit is invalid.";
    return false;
  }
  if (!parseLongStrict(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be between 0 and 15.";
    return false;
  }

  settings_.flightLat = lat;
  settings_.flightLon = lon;
  settings_.flightRadius = radius;
  settings_.flightRadiusUnit = static_cast<uint8_t>(radiusUnit);
  settings_.flightSpeedUnit = static_cast<uint8_t>(speedUnit);
  settings_.brightness = static_cast<uint8_t>(brightness);
  return true;
}

void FlightTrackerProject::drawRadar() {
  if (!runtime_.hasColor()) {
    Serial.println("Flight tracker display is missing color.");
    started_ = false;
    return;
  }

  runtime_.setBrightness(settings_.brightness);
  radar::draw(runtime_, adsb::tracks(), adsb::trackCount(),
              adsb::radiusNm(settings_), settings_.flightLat,
              settings_.flightLon, settings_.brightness);
  started_ = true;
}

void FlightTrackerProject::startPrograms() {
  lastFrameMs_ = 0;
  adsb::start(settings_);
  drawRadar();
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
  adsb::tick(settings_, dtMs);
  drawRadar();
}
