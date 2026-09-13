#pragma once

#include <stdint.h>

constexpr uint8_t FLIGHT_RADIUS_UNIT_MI = 0;
constexpr uint8_t FLIGHT_RADIUS_UNIT_KM = 1;
constexpr uint8_t FLIGHT_SPEED_UNIT_KT = 0;
constexpr uint8_t FLIGHT_SPEED_UNIT_MPH = 1;
constexpr uint8_t FLIGHT_SPEED_UNIT_KPH = 2;

constexpr float DEFAULT_FLIGHT_LAT = 40.6413f;
constexpr float DEFAULT_FLIGHT_LON = -73.7781f;
constexpr float DEFAULT_FLIGHT_RADIUS = 25.0f;
constexpr uint8_t DEFAULT_FLIGHT_RADIUS_UNIT = FLIGHT_RADIUS_UNIT_MI;
constexpr uint8_t DEFAULT_FLIGHT_SPEED_UNIT = FLIGHT_SPEED_UNIT_KT;
constexpr uint8_t DEFAULT_BRIGHTNESS = 4;

constexpr float MAX_FLIGHT_RADIUS_NM = 250.0f;

struct FlightTrackerSettings {
  float flightLat = DEFAULT_FLIGHT_LAT;
  float flightLon = DEFAULT_FLIGHT_LON;
  float flightRadius = DEFAULT_FLIGHT_RADIUS;
  uint8_t flightRadiusUnit = DEFAULT_FLIGHT_RADIUS_UNIT;
  uint8_t flightSpeedUnit = DEFAULT_FLIGHT_SPEED_UNIT;
  uint8_t brightness = DEFAULT_BRIGHTNESS;
};
