#pragma once

#include <MD_Parola.h>
#include <WString.h>

extern MD_Parola Display;

enum class ProgramId : uint8_t { Scroller, Fireworks, MazeHero, PixelArt };

constexpr uint8_t PROGRAM_SCROLLER_FLAG = 1U << 0;
constexpr uint8_t PROGRAM_FIREWORKS_FLAG = 1U << 1;
constexpr uint8_t PROGRAM_MAZE_HERO_FLAG = 1U << 2;
constexpr uint8_t PROGRAM_PIXEL_ART_FLAG = 1U << 3;
constexpr uint8_t PROGRAM_ALL_FLAGS =
    PROGRAM_SCROLLER_FLAG | PROGRAM_FIREWORKS_FLAG | PROGRAM_MAZE_HERO_FLAG |
    PROGRAM_PIXEL_ART_FLAG;
constexpr uint8_t DEFAULT_SELECTED_PROGRAMS =
    PROGRAM_FIREWORKS_FLAG | PROGRAM_MAZE_HERO_FLAG;
constexpr float DEFAULT_PROGRAM_DURATION_MINUTES = 5.0f;
constexpr float MAX_PROGRAM_DURATION_MINUTES = 43200.0f;
constexpr size_t MAX_SCROLL_MESSAGE_LENGTH = 64;

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
};

ProgramId parseProgramId(const String &value);
const char *programIdToString(ProgramId id);
uint8_t programIdToFlag(ProgramId id);
void programStart(const ProgramConfig &cfg);
void programTick(const ProgramConfig &cfg);
