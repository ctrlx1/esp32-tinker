#include "weather_watch_project.h"

#include "app_version.h"
#include "programs/program.h"
#include "programs/weather_watch/weather_watch.h"
#include "setup_html.h"

namespace {

constexpr uint16_t kSettingsVersion = 1;

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

bool parseBrightness(const String &value, uint8_t &result) {
  if (value.length() == 0 || value.length() > 2) {
    return false;
  }

  uint16_t parsed = 0;
  for (size_t i = 0; i < value.length(); i++) {
    const char character = value.charAt(i);
    if (character < '0' || character > '9') {
      return false;
    }
    parsed = static_cast<uint16_t>(parsed * 10 + character - '0');
  }

  if (parsed > WEATHER_WATCH_MAX_BRIGHTNESS) {
    return false;
  }
  result = static_cast<uint8_t>(parsed);
  return true;
}

} // namespace

WeatherWatchProject::WeatherWatchProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition WeatherWatchProject::definition() const {
  return {"weather_watch",
          "Weather Watch",
          APP_VERSION,
          "Weather-Watch-",
          {"weather_watch", "cfgVer", kSettingsVersion}};
}

bool WeatherWatchProject::migrateSettings(Preferences &,
                                          uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void WeatherWatchProject::loadSettings(Preferences &preferences) {
  settings_.brightness =
      preferences.getUChar("brightness", WEATHER_WATCH_DEFAULT_BRIGHTNESS);
  if (settings_.brightness > WEATHER_WATCH_MAX_BRIGHTNESS) {
    settings_.brightness = WEATHER_WATCH_DEFAULT_BRIGHTNESS;
  }

  Serial.print(" weather watch brightness=");
  Serial.println(settings_.brightness);
}

void WeatherWatchProject::saveSettings(Preferences &preferences) const {
  preferences.putUChar("brightness", settings_.brightness);
}

uint8_t WeatherWatchProject::displayBrightness() const {
  return settings_.brightness;
}

String WeatherWatchProject::buildPortalPage(const String &ip,
                                            const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool WeatherWatchProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("brightness")) {
    error = "Missing brightness.";
    return false;
  }

  uint8_t brightness;
  if (!parseBrightness(request.arg("brightness"), brightness)) {
    error = "Brightness must be a whole number from 0 to 15.";
    return false;
  }

  settings_.brightness = brightness;
  return true;
}

void WeatherWatchProject::startPrograms() {
  setProgramRuntimeContext(runtime_);
  weatherWatchStart(settings_);
  started_ = true;
}

void WeatherWatchProject::tickPrograms() {
  if (!started_) {
    startPrograms();
    return;
  }
  weatherWatchTick(settings_);
}
