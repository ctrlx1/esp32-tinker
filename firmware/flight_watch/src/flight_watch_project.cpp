#include "flight_watch_project.h"

#include "app_version.h"
#include "programs/flight_watch/flight_watch.h"
#include "programs/program.h"
#include "setup_html.h"

#include <cmath>
#include <stdlib.h>

namespace {
constexpr uint16_t kSettingsVersion = 1;
constexpr float kMilesToNm = 0.868976f;
constexpr float kKmToNm = 0.539957f;

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
  return radius * (unit == FLIGHT_RADIUS_UNIT_KM ? kKmToNm : kMilesToNm);
}

bool validRadius(float radius, uint8_t unit) {
  return std::isfinite(radius) && radius > 0.0f &&
         (unit == FLIGHT_RADIUS_UNIT_MI || unit == FLIGHT_RADIUS_UNIT_KM) &&
         radiusToNm(radius, unit) <= MAX_FLIGHT_RADIUS_NM;
}
} // namespace

FlightWatchProject::FlightWatchProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {
  setProgramRuntimeContext(runtime_);
}

tinker::ProjectDefinition FlightWatchProject::definition() const {
  return {"flight_watch",
          "Flight Watch",
          APP_VERSION,
          "Flight-Watch-",
          {"flight_watch", "cfgVer", kSettingsVersion}};
}

bool FlightWatchProject::migrateSettings(Preferences &, uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void FlightWatchProject::loadSettings(Preferences &preferences) {
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

  const uint32_t speed =
      preferences.getUInt("scrollSpeed", DEFAULT_SCROLL_SPEED_MS);
  settings_.scrollSpeedMs =
      speed >= MIN_SCROLL_SPEED_MS && speed <= MAX_SCROLL_SPEED_MS
          ? static_cast<uint16_t>(speed)
          : DEFAULT_SCROLL_SPEED_MS;
}

void FlightWatchProject::saveSettings(Preferences &preferences) const {
  preferences.putFloat("fltLat", settings_.flightLat);
  preferences.putFloat("fltLon", settings_.flightLon);
  preferences.putFloat("fltRad", settings_.flightRadius);
  preferences.putUChar("fltRadUnit", settings_.flightRadiusUnit);
  preferences.putUChar("fltSpdUnit", settings_.flightSpeedUnit);
  preferences.putUChar("brightness", settings_.brightness);
  preferences.putUInt("scrollSpeed", settings_.scrollSpeedMs);
}

uint8_t FlightWatchProject::displayBrightness() const {
  return settings_.brightness;
}

String FlightWatchProject::buildPortalPage(const String &ip,
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
  page.replace("SCROLL_SPEED_PLACEHOLDER", String(settings_.scrollSpeedMs));
  return page;
}

bool FlightWatchProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("lat") || !request.hasArg("lon") ||
      !request.hasArg("radius") || !request.hasArg("radiusUnit") ||
      !request.hasArg("speedUnit") || !request.hasArg("brightness") ||
      !request.hasArg("scrollSpeed")) {
    error = "Missing Flight Watch settings.";
    return false;
  }

  float lat;
  float lon;
  float radius;
  long radiusUnit;
  long speedUnit;
  long brightness;
  long scrollSpeed;
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
  if (!parseLongStrict(request.arg("scrollSpeed"), MIN_SCROLL_SPEED_MS,
                       MAX_SCROLL_SPEED_MS, scrollSpeed)) {
    error = "Scroll speed must be between 1 and 10000 ms.";
    return false;
  }

  settings_.flightLat = lat;
  settings_.flightLon = lon;
  settings_.flightRadius = radius;
  settings_.flightRadiusUnit = static_cast<uint8_t>(radiusUnit);
  settings_.flightSpeedUnit = static_cast<uint8_t>(speedUnit);
  settings_.brightness = static_cast<uint8_t>(brightness);
  settings_.scrollSpeedMs = static_cast<uint16_t>(scrollSpeed);
  return true;
}

void FlightWatchProject::startPrograms() { flightWatchStart(settings_); }

void FlightWatchProject::tickPrograms() { flightWatchTick(settings_); }
