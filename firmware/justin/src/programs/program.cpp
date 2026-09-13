#include "program.h"

#include <cassert>

namespace {
tinker::RuntimeContext *runtimeContext = nullptr;
}

void setProgramRuntimeContext(tinker::RuntimeContext &runtime) {
  assert(runtimeContext == nullptr || runtimeContext == &runtime);
  runtimeContext = &runtime;
}

tinker::RuntimeContext &programRuntimeContext() {
  assert(runtimeContext != nullptr);
  return *runtimeContext;
}

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
