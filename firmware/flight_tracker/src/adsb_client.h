#pragma once

#include "flight_tracker_settings.h"

#include <stdint.h>

namespace adsb {

constexpr uint8_t kMaxTracks = 24;

struct TrackedAircraft {
  char hex[7];
  char callsign[9];
  char typeCode[5];
  float lat;
  float lon;
  float eastNm;
  float northNm;
  int16_t gsKt;
  int16_t trackDeg;
  int32_t altFt;
};

float radiusNm(const FlightTrackerSettings &settings);

void start(const FlightTrackerSettings &settings);
void tick(const FlightTrackerSettings &settings, unsigned long dtMs);

const TrackedAircraft *tracks();
uint8_t trackCount();

} // namespace adsb
