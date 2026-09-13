#pragma once

#include <Preferences.h>
#include <tinker/descriptor_scheduler.h>
#include <tinker/portal_request.h>
#include <tinker/project_definition.h>

#include "programs/program.h"

class JustinProject {
public:
  explicit JustinProject(tinker::RuntimeContext &runtime);

  tinker::ProjectDefinition definition() const;
  bool migrateSettings(Preferences &preferences, uint16_t storedVersion);

  void loadSettings(Preferences &preferences);
  void saveSettings(Preferences &preferences) const;
  uint8_t displayBrightness() const;

  String buildPortalPage(const String &ip, const String &ssid) const;
  bool applyPortalRequest(const tinker::PortalRequest &request, String &error);

  void startPrograms();
  void tickPrograms();
  const JustinSettings &settings() const { return config_; }

private:
  static void startTransition(void *context);
  static bool tickTransition(void *context);
  static uint32_t nowMs(void *context);

  tinker::SchedulerBindings schedulerBindings();

  static const tinker::ProgramDescriptor kPrograms[4];
  JustinSettings config_;
  tinker::DescriptorScheduler<uint8_t> scheduler_;
};
