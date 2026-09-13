#include "moon_phase_project.h"

#include "app_version.h"
#include "programs/moon_phase/moon_phase.h"
#include "setup_html.h"

namespace {
constexpr uint16_t kSettingsVersion = 1;
constexpr uint8_t kDefaultBrightness = 0;
constexpr uint16_t kDefaultScrollSpeedMs = 75;

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

bool parseUnsignedInRange(String value, uint32_t minimum, uint32_t maximum,
                          uint32_t &result) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }

  uint32_t parsed = 0;
  for (size_t i = 0; i < value.length(); i++) {
    const char digit = value.charAt(i);
    if (digit < '0' || digit > '9') {
      return false;
    }
    const uint32_t next = parsed * 10U + static_cast<uint32_t>(digit - '0');
    if (next < parsed || next > maximum) {
      return false;
    }
    parsed = next;
  }

  if (parsed < minimum) {
    return false;
  }
  result = parsed;
  return true;
}
} // namespace

MoonPhaseProject::MoonPhaseProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {
  setProgramRuntimeContext(runtime_);
}

tinker::ProjectDefinition MoonPhaseProject::definition() const {
  return {"moon_phase",
          "Moon Phase",
          APP_VERSION,
          "Moon-Phase-",
          {"moon_phase", "cfgVer", kSettingsVersion}};
}

bool MoonPhaseProject::migrateSettings(Preferences &, uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void MoonPhaseProject::loadSettings(Preferences &preferences) {
  settings_.brightness =
      preferences.getUChar("brightness", kDefaultBrightness);
  if (settings_.brightness > 15) {
    settings_.brightness = kDefaultBrightness;
  }

  const uint32_t storedScrollSpeed =
      preferences.getUInt("scrollSpeed", kDefaultScrollSpeedMs);
  settings_.scrollSpeedMs =
      storedScrollSpeed >= MOON_PHASE_MIN_SCROLL_SPEED_MS &&
              storedScrollSpeed <= MOON_PHASE_MAX_SCROLL_SPEED_MS
          ? static_cast<uint16_t>(storedScrollSpeed)
          : kDefaultScrollSpeedMs;

  Serial.print(" brightness=");
  Serial.print(settings_.brightness);
  Serial.print(" phase-name speed=");
  Serial.println(settings_.scrollSpeedMs);
}

void MoonPhaseProject::saveSettings(Preferences &preferences) const {
  preferences.putUChar("brightness", settings_.brightness);
  preferences.putUInt("scrollSpeed", settings_.scrollSpeedMs);
}

uint8_t MoonPhaseProject::displayBrightness() const {
  return settings_.brightness;
}

String MoonPhaseProject::buildPortalPage(const String &ip,
                                         const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("SPEED_PLACEHOLDER", String(settings_.scrollSpeedMs));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool MoonPhaseProject::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("speed") || !request.hasArg("brightness")) {
    error = "Missing moon_phase settings.";
    return false;
  }

  uint32_t speed;
  if (!parseUnsignedInRange(request.arg("speed"),
                            MOON_PHASE_MIN_SCROLL_SPEED_MS,
                            MOON_PHASE_MAX_SCROLL_SPEED_MS, speed)) {
    error = "Phase-name scroll speed must be an integer from 1 to 10000 ms.";
    return false;
  }

  uint32_t brightness;
  if (!parseUnsignedInRange(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be an integer from 0 to 15.";
    return false;
  }

  settings_.scrollSpeedMs = static_cast<uint16_t>(speed);
  settings_.brightness = static_cast<uint8_t>(brightness);
  return true;
}

void MoonPhaseProject::startPrograms() {
  moonPhaseStart(settings_);
  started_ = true;
}

void MoonPhaseProject::tickPrograms() {
  if (!started_) {
    startPrograms();
  }
  moonPhaseTick(settings_);
}
