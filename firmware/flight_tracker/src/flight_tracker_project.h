#pragma once

#include "flight_tracker_settings.h"

#include <Preferences.h>
#include <tinker/portal_request.h>
#include <tinker/project_definition.h>
#include <tinker/runtime_context.h>

class FlightTrackerProject {
public:
  explicit FlightTrackerProject(tinker::RuntimeContext &runtime);

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
  void drawRadar();

  tinker::RuntimeContext &runtime_;
  FlightTrackerSettings settings_;
  bool started_ = false;
  unsigned long lastFrameMs_ = 0;
};
