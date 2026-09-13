#include "starter_project.h"

#include "app_version.h"
#include "setup_html.h"

#include <stdlib.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr uint8_t kDefaultBrightness = 4;
constexpr uint16_t kDefaultScrollSpeedMs = 75;
constexpr const char *kDefaultMessage = "Hello World";

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

StarterProject::StarterProject(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition StarterProject::definition() const {
  return {"starter-max7219",
          "Starter MAX7219",
          APP_VERSION,
          "Starter-M7219-",
          {"starter_m7219", "cfgVer", kSettingsVersion}};
}

bool StarterProject::migrateSettings(Preferences &, uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void StarterProject::loadSettings(Preferences &preferences) {
  settings_.message = preferences.getString("message", kDefaultMessage);
  if (settings_.message.length() == 0 ||
      settings_.message.length() > STARTER_MESSAGE_MAX_LENGTH) {
    settings_.message = kDefaultMessage;
  }

  settings_.scrollSpeedMs =
      preferences.getUInt("speed", kDefaultScrollSpeedMs);
  if (settings_.scrollSpeedMs < STARTER_MIN_SCROLL_SPEED_MS ||
      settings_.scrollSpeedMs > STARTER_MAX_SCROLL_SPEED_MS) {
    settings_.scrollSpeedMs = kDefaultScrollSpeedMs;
  }

  settings_.brightness =
      preferences.getUChar("brightness", kDefaultBrightness);
  if (settings_.brightness > 15) {
    settings_.brightness = kDefaultBrightness;
  }

  Serial.print(" starter message=");
  Serial.print(settings_.message);
  Serial.print(" speed=");
  Serial.print(settings_.scrollSpeedMs);
  Serial.print(" brightness=");
  Serial.println(settings_.brightness);
}

void StarterProject::saveSettings(Preferences &preferences) const {
  preferences.putString("message", settings_.message);
  preferences.putUInt("speed", settings_.scrollSpeedMs);
  preferences.putUChar("brightness", settings_.brightness);
}

uint8_t StarterProject::displayBrightness() const {
  return settings_.brightness;
}

String StarterProject::buildPortalPage(const String &ip,
                                       const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("MESSAGE_PLACEHOLDER", htmlEscape(settings_.message));
  page.replace("SPEED_PLACEHOLDER", String(settings_.scrollSpeedMs));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  return page;
}

bool StarterProject::applyPortalRequest(const tinker::PortalRequest &request,
                                        String &error) {
  if (!request.hasArg("message") || !request.hasArg("speed") ||
      !request.hasArg("brightness")) {
    error = "Missing starter settings.";
    return false;
  }

  String message = request.arg("message");
  if (message.length() == 0) {
    message = kDefaultMessage;
  }
  if (message.length() > STARTER_MESSAGE_MAX_LENGTH) {
    error = "Message must be 64 characters or fewer.";
    return false;
  }

  long speed;
  if (!parseLongInRange(request.arg("speed"), STARTER_MIN_SCROLL_SPEED_MS,
                        STARTER_MAX_SCROLL_SPEED_MS, speed)) {
    error = "Scroll speed must be between 1 and 10000 ms.";
    return false;
  }

  long brightness;
  if (!parseLongInRange(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be between 0 and 15.";
    return false;
  }

  settings_.message = message;
  settings_.scrollSpeedMs = static_cast<uint16_t>(speed);
  settings_.brightness = static_cast<uint8_t>(brightness);
  return true;
}

void StarterProject::startHelloWorld() {
  const char *message = settings_.message.length() > 0
                            ? settings_.message.c_str()
                            : kDefaultMessage;
  if (!runtime_.copyText(message, STARTER_MESSAGE_MAX_LENGTH)) {
    Serial.println("Starter display has no text buffer.");
    started_ = false;
    return;
  }

  runtime_.setBrightness(settings_.brightness);
  runtime_.clearText();
  runtime_.startTextScroll(runtime_.textBuffer(), tinker::TextAlignment::Left,
                           settings_.scrollSpeedMs);
  started_ = true;
}

void StarterProject::startPrograms() { startHelloWorld(); }

void StarterProject::tickPrograms() {
  if (!started_) {
    startHelloWorld();
    return;
  }
  if (runtime_.animateText()) {
    runtime_.resetTextAnimation();
  }
}
