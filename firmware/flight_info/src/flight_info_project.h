#pragma once

#include "adsb_client.h"
#include "flight_info_settings.h"

#include <Preferences.h>
#include <tinker/portal_request.h>
#include <tinker/project_definition.h>
#include <tinker/runtime_context.h>

class FlightInfoProject {
public:
  explicit FlightInfoProject(tinker::RuntimeContext &runtime);

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
  void showStatus(const char *message);
  void drawCurrent();
  bool cardUnchanged(const adsb::Aircraft &aircraft, uint8_t count) const;
  void rememberCard(const adsb::Aircraft &aircraft, uint8_t count);

  tinker::RuntimeContext &runtime_;
  FlightInfoSettings settings_;
  adsb::Aircraft lastAircraft_{};
  bool started_ = false;
  bool showingCard_ = false;
  bool haveLastCard_ = false;
  uint8_t featuredIndex_ = 0;
  uint8_t lastCardIndex_ = 0;
  uint8_t lastCardCount_ = 0;
  uint8_t lastSpeedUnit_ = 0;
  uint8_t lastDistanceUnit_ = 0;
  unsigned long lastFrameMs_ = 0;
  unsigned long lastCardMs_ = 0;
  String lastStatus_;
};
