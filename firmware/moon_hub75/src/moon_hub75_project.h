#pragma once

#include "moon_hub75_settings.h"

#include <Preferences.h>
#include <tinker/portal_request.h>
#include <tinker/project_definition.h>
#include <tinker/runtime_context.h>

class MoonHub75Project {
public:
  explicit MoonHub75Project(tinker::RuntimeContext &runtime);

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
  void updatePhase(unsigned long now);
  void rebuildLines();
  void drawFrame();

  tinker::RuntimeContext &runtime_;
  MoonHub75Settings settings_;
  bool started_ = false;
  bool timeSynced_ = false;
  bool ntpAttempted_ = false;
  unsigned long lastNtpAttemptMs_ = 0;
  unsigned long lastScrollMs_ = 0;
  float phase_ = 0.0f;
  uint8_t illuminationPercent_ = 0;
  int scrollOffset_ = 0;
  uint8_t lineCount_ = 0;
  int contentHeight_ = 0;
  char lines_[48][9] = {};
};
