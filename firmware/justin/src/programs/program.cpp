#include "program.h"

#include "fireworks.h"
#include "flight_watch.h"
#include "maze_hero.h"
#include "moon_phase.h"
#include "pixel_art.h"
#include "real_weather.h"
#include "scroller.h"
#include "weather_watch.h"

char gProgramScrollBuffer[PROGRAM_SCROLL_BUFFER_SIZE];

ProgramId parseProgramId(const String &value) {
  if (value == "fireworks") {
    return ProgramId::Fireworks;
  }
  if (value == "maze_hero") {
    return ProgramId::MazeHero;
  }
  if (value == "pixel_art") {
    return ProgramId::PixelArt;
  }
  if (value == "weather_watch") {
    return ProgramId::WeatherWatch;
  }
  if (value == "real_weather") {
    return ProgramId::RealWeather;
  }
  if (value == "moon_phase") {
    return ProgramId::MoonPhase;
  }
  if (value == "flight_watch") {
    return ProgramId::FlightWatch;
  }
  return ProgramId::Scroller;
}

const char *programIdToString(ProgramId id) {
  switch (id) {
  case ProgramId::Fireworks:
    return "fireworks";
  case ProgramId::MazeHero:
    return "maze_hero";
  case ProgramId::PixelArt:
    return "pixel_art";
  case ProgramId::WeatherWatch:
    return "weather_watch";
  case ProgramId::RealWeather:
    return "real_weather";
  case ProgramId::MoonPhase:
    return "moon_phase";
  case ProgramId::FlightWatch:
    return "flight_watch";
  case ProgramId::Scroller:
  default:
    return "scroller";
  }
}

uint8_t programIdToFlag(ProgramId id) {
  switch (id) {
  case ProgramId::Fireworks:
    return PROGRAM_FIREWORKS_FLAG;
  case ProgramId::MazeHero:
    return PROGRAM_MAZE_HERO_FLAG;
  case ProgramId::PixelArt:
    return PROGRAM_PIXEL_ART_FLAG;
  case ProgramId::WeatherWatch:
    return PROGRAM_WEATHER_WATCH_FLAG;
  case ProgramId::RealWeather:
    return PROGRAM_REAL_WEATHER_FLAG;
  case ProgramId::MoonPhase:
    return PROGRAM_MOON_PHASE_FLAG;
  case ProgramId::FlightWatch:
    return PROGRAM_FLIGHT_WATCH_FLAG;
  case ProgramId::Scroller:
  default:
    return PROGRAM_SCROLLER_FLAG;
  }
}

void programStart(const ProgramConfig &cfg) { programStart(cfg.program, cfg); }

void programStart(ProgramId id, const ProgramConfig &cfg) {
  switch (id) {
  case ProgramId::Fireworks:
    fireworksStart(cfg);
    break;
  case ProgramId::MazeHero:
    mazeHeroStart(cfg);
    break;
  case ProgramId::PixelArt:
    pixelArtStart(cfg);
    break;
  case ProgramId::WeatherWatch:
    weatherWatchStart(cfg);
    break;
  case ProgramId::RealWeather:
    realWeatherStart(cfg);
    break;
  case ProgramId::MoonPhase:
    moonPhaseStart(cfg);
    break;
  case ProgramId::FlightWatch:
    flightWatchStart(cfg);
    break;
  case ProgramId::Scroller:
  default:
    scrollerStart(cfg);
    break;
  }
}

void programTick(const ProgramConfig &cfg) { programTick(cfg.program, cfg); }

void programTick(ProgramId id, const ProgramConfig &cfg) {
  switch (id) {
  case ProgramId::Fireworks:
    fireworksTick(cfg);
    break;
  case ProgramId::MazeHero:
    mazeHeroTick(cfg);
    break;
  case ProgramId::PixelArt:
    pixelArtTick(cfg);
    break;
  case ProgramId::WeatherWatch:
    weatherWatchTick(cfg);
    break;
  case ProgramId::RealWeather:
    realWeatherTick(cfg);
    break;
  case ProgramId::MoonPhase:
    moonPhaseTick(cfg);
    break;
  case ProgramId::FlightWatch:
    flightWatchTick(cfg);
    break;
  case ProgramId::Scroller:
  default:
    scrollerTick(cfg);
    break;
  }
}
