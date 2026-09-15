#include "flight_info_project.h"

#include "adsb_client.h"
#include "app_version.h"
#include "card_view.h"
#include "setup_html.h"

#include <cmath>
#include <stdlib.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
#ifdef WOKWI_SIM
constexpr unsigned long kFrameIntervalMs = 80;
#else
constexpr unsigned long kFrameIntervalMs = 16;
#endif

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

const char *statusMessage(adsb::Status status) {
  switch (status) {
  case adsb::Status::Loading:
    return "Loading\nflights...";
  case adsb::Status::Success:
    return "No planes\nnearby";
  case adsb::Status::WifiUnavailable:
    return "Flight\noffline";
  case adsb::Status::ParseFailed:
    return "Parse\nfailed";
  case adsb::Status::HttpFailed:
  default:
    return "Fetch\nfailed";
  }
}

} // namespace

FlightInfoProject::FlightInfoProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition FlightInfoProject::definition() const {
  return {"flight_info",
          "Flight Info",
          APP_VERSION,
          "FlightInfo-",
          {"flight_info", "cfgVer", kSettingsVersion}};
}

bool FlightInfoProject::migrateSettings(Preferences &,
                                        uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void FlightInfoProject::loadSettings(Preferences &preferences) {
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

  settings_.flightDistanceUnit =
      preferences.getUChar("dstUnit", DEFAULT_FLIGHT_DISTANCE_UNIT);
  if (settings_.flightDistanceUnit > FLIGHT_DISTANCE_UNIT_KM) {
    settings_.flightDistanceUnit = DEFAULT_FLIGHT_DISTANCE_UNIT;
  }

  settings_.brightness =
      preferences.getUChar("brightness", DEFAULT_BRIGHTNESS);
  if (settings_.brightness > 15) {
    settings_.brightness = DEFAULT_BRIGHTNESS;
  }

  settings_.cardDwellSec =
      preferences.getUChar("cardDwell", DEFAULT_CARD_DWELL_SEC);
  if (settings_.cardDwellSec < MIN_CARD_DWELL_SEC ||
      settings_.cardDwellSec > MAX_CARD_DWELL_SEC) {
    settings_.cardDwellSec = DEFAULT_CARD_DWELL_SEC;
  }

  Serial.print(" flight_info lat=");
  Serial.print(settings_.flightLat, 5);
  Serial.print(" lon=");
  Serial.print(settings_.flightLon, 5);
  Serial.print(" radius=");
  Serial.print(settings_.flightRadius);
  Serial.print(" dwell=");
  Serial.print(settings_.cardDwellSec);
  Serial.print(" brightness=");
  Serial.println(settings_.brightness);
}

void FlightInfoProject::saveSettings(Preferences &preferences) const {
  preferences.putFloat("fltLat", settings_.flightLat);
  preferences.putFloat("fltLon", settings_.flightLon);
  preferences.putFloat("fltRad", settings_.flightRadius);
  preferences.putUChar("fltRadUnit", settings_.flightRadiusUnit);
  preferences.putUChar("fltSpdUnit", settings_.flightSpeedUnit);
  preferences.putUChar("dstUnit", settings_.flightDistanceUnit);
  preferences.putUChar("brightness", settings_.brightness);
  preferences.putUChar("cardDwell", settings_.cardDwellSec);
}

uint8_t FlightInfoProject::displayBrightness() const {
  return settings_.brightness;
}

String FlightInfoProject::buildPortalPage(const String &ip,
                                          const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("LAT_PLACEHOLDER", String(settings_.flightLat, 6));
  page.replace("LON_PLACEHOLDER", String(settings_.flightLon, 6));
  page.replace("RADIUS_PLACEHOLDER", String(settings_.flightRadius, 2));
  page.replace("RADIUS_UNIT_PLACEHOLDER", String(settings_.flightRadiusUnit));
  page.replace("SPEED_UNIT_PLACEHOLDER", String(settings_.flightSpeedUnit));
  page.replace("DIST_UNIT_PLACEHOLDER", String(settings_.flightDistanceUnit));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  page.replace("DWELL_PLACEHOLDER", String(settings_.cardDwellSec));
  return page;
}

bool FlightInfoProject::applyPortalRequest(const tinker::PortalRequest &request,
                                           String &error) {
  if (!request.hasArg("lat") || !request.hasArg("lon") ||
      !request.hasArg("radius") || !request.hasArg("radiusUnit") ||
      !request.hasArg("speedUnit") || !request.hasArg("distUnit") ||
      !request.hasArg("brightness") || !request.hasArg("cardDwell")) {
    error = "Missing flight info settings.";
    return false;
  }

  float lat;
  float lon;
  float radius;
  long radiusUnit;
  long speedUnit;
  long distUnit;
  long brightness;
  long cardDwell;
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
  if (!parseLongStrict(request.arg("distUnit"), FLIGHT_DISTANCE_UNIT_MI,
                       FLIGHT_DISTANCE_UNIT_KM, distUnit)) {
    error = "Distance unit is invalid.";
    return false;
  }
  if (!parseLongStrict(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be between 0 and 15.";
    return false;
  }
  if (!parseLongStrict(request.arg("cardDwell"), MIN_CARD_DWELL_SEC,
                       MAX_CARD_DWELL_SEC, cardDwell)) {
    error = "Card dwell must be between 2 and 30 seconds.";
    return false;
  }

  settings_.flightLat = lat;
  settings_.flightLon = lon;
  settings_.flightRadius = radius;
  settings_.flightRadiusUnit = static_cast<uint8_t>(radiusUnit);
  settings_.flightSpeedUnit = static_cast<uint8_t>(speedUnit);
  settings_.flightDistanceUnit = static_cast<uint8_t>(distUnit);
  settings_.brightness = static_cast<uint8_t>(brightness);
  settings_.cardDwellSec = static_cast<uint8_t>(cardDwell);
  return true;
}

void FlightInfoProject::showStatus(const char *message) {
  if (!runtime_.hasText() || !runtime_.hasColor()) {
    Serial.println("Flight info display is missing text or color.");
    started_ = false;
    return;
  }
  if (showingCard_ || lastStatus_ != message) {
    runtime_.setBrightness(settings_.brightness);
    runtime_.setTextColor(200, 220, 255);
    runtime_.clearText();
    runtime_.showMessage(message);
    lastStatus_ = message;
    showingCard_ = false;
  }
  started_ = true;
}

void FlightInfoProject::drawCurrent() {
  if (!runtime_.hasColor() || !runtime_.hasRgb565Blit()) {
    Serial.println("Flight info display is missing color.");
    started_ = false;
    return;
  }

  const uint8_t count = adsb::count();
  if (count == 0) {
    if (adsb::status() == adsb::Status::Success) {
      card::drawIdleRadar(runtime_, settings_.brightness);
      showingCard_ = false;
      lastStatus_ = "";
      started_ = true;
    } else {
      showStatus(statusMessage(adsb::status()));
    }
    return;
  }

  if (featuredIndex_ >= count) {
    featuredIndex_ = 0;
  }
  card::draw(runtime_, adsb::items()[featuredIndex_], featuredIndex_, count,
             settings_.flightSpeedUnit, settings_.flightDistanceUnit,
             settings_.brightness);
  showingCard_ = true;
  lastStatus_ = "";
  started_ = true;
}

void FlightInfoProject::startPrograms() {
  lastFrameMs_ = 0;
  lastCardMs_ = millis();
  featuredIndex_ = 0;
  showingCard_ = false;
  lastStatus_ = "";
  adsb::start(settings_);
  drawCurrent();
}

void FlightInfoProject::tickPrograms() {
  const unsigned long now = millis();
  if (started_ && lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;

  const uint8_t previousCount = adsb::count();
  adsb::tick(settings_);
  const uint8_t count = adsb::count();
  if (count == 0) {
    featuredIndex_ = 0;
    lastCardMs_ = now;
    drawCurrent();
    return;
  }

  if (previousCount == 0 || featuredIndex_ >= count) {
    featuredIndex_ = 0;
    lastCardMs_ = now;
  } else {
    const unsigned long dwellMs =
        static_cast<unsigned long>(settings_.cardDwellSec) * 1000UL;
    if (count > 1 && now - lastCardMs_ >= dwellMs) {
      featuredIndex_ = static_cast<uint8_t>((featuredIndex_ + 1) % count);
      lastCardMs_ = now;
    }
  }

  drawCurrent();
}
