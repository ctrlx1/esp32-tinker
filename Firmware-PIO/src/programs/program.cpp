#include "program.h"

#include "fireworks.h"
#include "maze_hero.h"
#include "pixel_art.h"
#include "scroller.h"

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
  case ProgramId::Scroller:
  default:
    return PROGRAM_SCROLLER_FLAG;
  }
}

void programStart(const ProgramConfig &cfg) {
  switch (cfg.program) {
  case ProgramId::Fireworks:
    fireworksStart(cfg);
    break;
  case ProgramId::MazeHero:
    mazeHeroStart(cfg);
    break;
  case ProgramId::PixelArt:
    pixelArtStart(cfg);
    break;
  case ProgramId::Scroller:
  default:
    scrollerStart(cfg);
    break;
  }
}

void programTick(const ProgramConfig &cfg) {
  switch (cfg.program) {
  case ProgramId::Fireworks:
    fireworksTick(cfg);
    break;
  case ProgramId::MazeHero:
    mazeHeroTick(cfg);
    break;
  case ProgramId::PixelArt:
    pixelArtTick(cfg);
    break;
  case ProgramId::Scroller:
  default:
    scrollerTick(cfg);
    break;
  }
}
