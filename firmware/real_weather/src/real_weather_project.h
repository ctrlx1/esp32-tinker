#pragma once

#include "real_weather_settings.h"

#include <Preferences.h>
#include <tinker/portal_request.h>
#include <tinker/project_definition.h>
#include <tinker/runtime_context.h>

class RealWeatherProject {
public:
  explicit RealWeatherProject(tinker::RuntimeContext &runtime);

  tinker::ProjectDefinition definition() const;
  bool migrateSettings(Preferences &preferences, uint16_t storedVersion);
  void loadSettings(Preferences &preferences);
  void saveSettings(Preferences &preferences) const;
  uint8_t displayBrightness() const;

  String buildPortalPage(const String &ip, const String &ssid) const;
  bool applyPortalRequest(const tinker::PortalRequest &request, String &error);

  void startPrograms();
  void tickPrograms();

private:
  tinker::RuntimeContext &runtime_;
  RealWeatherSettings settings_;
};
