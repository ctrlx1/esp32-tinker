#include "real_weather_project.h"

#include "app_version.h"
#include "programs/program.h"
#include "programs/real_weather/real_weather.h"
#include "setup_html.h"

#include <stdlib.h>

namespace {

constexpr uint16_t kSettingsVersion = 1;
constexpr uint16_t kDefaultScrollSpeedMs = 75;
constexpr uint8_t kDefaultBrightness = 0;

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

bool isValidPostalCode(const String &value) {
  if (value.length() == 0) {
    return true;
  }
  if (value.length() != 5 && value.length() != 10) {
    return false;
  }
  for (size_t i = 0; i < value.length(); ++i) {
    if (i == 5 && value.length() == 10) {
      if (value.charAt(i) != '-') {
        return false;
      }
    } else if (value.charAt(i) < '0' || value.charAt(i) > '9') {
      return false;
    }
  }
  return true;
}

} // namespace

RealWeatherProject::RealWeatherProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition RealWeatherProject::definition() const {
  return {"real_weather",
          "Real Weather",
          APP_VERSION,
          "Real-Weather-",
          {"real_weather", "cfgVer", kSettingsVersion}};
}

bool RealWeatherProject::migrateSettings(Preferences &, uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void RealWeatherProject::loadSettings(Preferences &preferences) {
  settings_.weatherPostalCode = preferences.getString("wxZip", "");
  settings_.weatherPostalCode.trim();
  if (!isValidPostalCode(settings_.weatherPostalCode)) {
    settings_.weatherPostalCode = "";
  }

  settings_.scrollSpeedMs =
      preferences.getUInt("scrollSpeed", kDefaultScrollSpeedMs);
  if (settings_.scrollSpeedMs < REAL_WEATHER_MIN_SCROLL_SPEED_MS ||
      settings_.scrollSpeedMs > REAL_WEATHER_MAX_SCROLL_SPEED_MS) {
    settings_.scrollSpeedMs = kDefaultScrollSpeedMs;
  }

  settings_.brightness =
      preferences.getUChar("brightness", kDefaultBrightness);
  if (settings_.brightness > 15) {
    settings_.brightness = kDefaultBrightness;
  }

  Serial.print(" wxZip=");
  Serial.print(settings_.weatherPostalCode.length()
                   ? settings_.weatherPostalCode
                   : "(empty)");
  Serial.print(" scrollSpeed=");
  Serial.print(settings_.scrollSpeedMs);
  Serial.print(" brightness=");
  Serial.println(settings_.brightness);
}

void RealWeatherProject::saveSettings(Preferences &preferences) const {
  preferences.putString("wxZip", settings_.weatherPostalCode);
  preferences.putUInt("scrollSpeed", settings_.scrollSpeedMs);
  preferences.putUChar("brightness", settings_.brightness);
}

uint8_t RealWeatherProject::displayBrightness() const {
  return settings_.brightness;
}

String RealWeatherProject::buildPortalPage(const String &ip,
                                           const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("WX_ZIP_PLACEHOLDER",
               htmlEscape(settings_.weatherPostalCode));
  page.replace("SPEED_PLACEHOLDER", String(settings_.scrollSpeedMs));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool RealWeatherProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("wxZip") || !request.hasArg("speed") ||
      !request.hasArg("brightness")) {
    error = "Missing weather settings.";
    return false;
  }

  String postalCode = request.arg("wxZip");
  postalCode.trim();
  if (!isValidPostalCode(postalCode)) {
    error = "ZIP code must be empty, 5 digits, or ZIP+4.";
    return false;
  }

  long speed = 0;
  if (!parseLongInRange(request.arg("speed"),
                        REAL_WEATHER_MIN_SCROLL_SPEED_MS,
                        REAL_WEATHER_MAX_SCROLL_SPEED_MS, speed)) {
    error = "Scroll speed must be an integer from 1 to 10000 ms.";
    return false;
  }

  long brightness = 0;
  if (!parseLongInRange(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be an integer from 0 to 15.";
    return false;
  }

  settings_.weatherPostalCode = postalCode;
  settings_.scrollSpeedMs = static_cast<uint16_t>(speed);
  settings_.brightness = static_cast<uint8_t>(brightness);
  return true;
}

void RealWeatherProject::startPrograms() {
  setProgramRuntimeContext(runtime_);
  realWeatherStart(settings_);
}

void RealWeatherProject::tickPrograms() { realWeatherTick(settings_); }
