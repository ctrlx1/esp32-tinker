#pragma once

#include <tinker/runtime_context.h>
#include <WString.h>

enum class ProgramId : uint8_t {
  Scroller,
  Fireworks,
  MazeHero,
  PixelArt,
  WeatherWatch,
  RealWeather,
  MoonPhase,
  FlightWatch
};

constexpr uint8_t PROGRAM_SCROLLER_FLAG = 1U << 0;
constexpr uint8_t PROGRAM_FIREWORKS_FLAG = 1U << 1;
constexpr uint8_t PROGRAM_MAZE_HERO_FLAG = 1U << 2;
constexpr uint8_t PROGRAM_PIXEL_ART_FLAG = 1U << 3;
constexpr uint8_t PROGRAM_WEATHER_WATCH_FLAG = 1U << 4;
constexpr uint8_t PROGRAM_REAL_WEATHER_FLAG = 1U << 5;
constexpr uint8_t PROGRAM_MOON_PHASE_FLAG = 1U << 6;
constexpr uint8_t PROGRAM_FLIGHT_WATCH_FLAG = 1U << 7;
constexpr uint8_t PROGRAM_ALL_FLAGS =
    PROGRAM_SCROLLER_FLAG | PROGRAM_FIREWORKS_FLAG | PROGRAM_MAZE_HERO_FLAG |
    PROGRAM_PIXEL_ART_FLAG | PROGRAM_WEATHER_WATCH_FLAG |
    PROGRAM_REAL_WEATHER_FLAG | PROGRAM_MOON_PHASE_FLAG |
    PROGRAM_FLIGHT_WATCH_FLAG;
constexpr uint8_t DEFAULT_SELECTED_PROGRAMS =
    PROGRAM_FIREWORKS_FLAG | PROGRAM_MAZE_HERO_FLAG;
constexpr float DEFAULT_PROGRAM_DURATION_MINUTES = 5.0f;
constexpr float MAX_PROGRAM_DURATION_MINUTES = 43200.0f;
constexpr size_t MAX_SCROLL_MESSAGE_LENGTH = 64;
constexpr size_t MAX_WEATHER_POSTAL_CODE_LENGTH = 16;
constexpr uint8_t FLIGHT_RADIUS_UNIT_MI = 0;
constexpr uint8_t FLIGHT_RADIUS_UNIT_KM = 1;
constexpr uint8_t FLIGHT_SPEED_UNIT_KT = 0;
constexpr uint8_t FLIGHT_SPEED_UNIT_MPH = 1;
constexpr uint8_t FLIGHT_SPEED_UNIT_KPH = 2;
constexpr float DEFAULT_FLIGHT_LAT = 40.6413f;
constexpr float DEFAULT_FLIGHT_LON = -73.7781f;
constexpr float DEFAULT_FLIGHT_RADIUS = 25.0f;
constexpr float MAX_FLIGHT_RADIUS_NM = 250.0f;

struct ProgramConfig {
  ProgramId program;
  uint8_t selectedPrograms;
  float programDurationMinutes;
  String scrollMessage;
  unsigned int scrollSpeedMs;
  unsigned int fireworksMinLaunchDelayMs;
  unsigned int fireworksMaxLaunchDelayMs;
  unsigned int fireworksAnimSpeedMs;
  unsigned int mazeMinWidth;
  unsigned int mazeMaxWidth;
  unsigned int mazeMinHeight;
  unsigned int mazeMaxHeight;
  unsigned int mazeHeroMinSpeedMs;
  unsigned int mazeHeroMaxSpeedMs;
  uint8_t brightness;
  uint8_t fireworksMaxBrightness;
  String weatherPostalCode;
  float flightLat;
  float flightLon;
  float flightRadius;
  uint8_t flightRadiusUnit;
  uint8_t flightSpeedUnit;
};

ProgramId parseProgramId(const String &value);
const char *programIdToString(ProgramId id);
uint8_t programIdToFlag(ProgramId id);
void programStart(const ProgramConfig &cfg);
void programTick(const ProgramConfig &cfg);
void programStart(ProgramId id, const ProgramConfig &cfg);
void programTick(ProgramId id, const ProgramConfig &cfg);
void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();

