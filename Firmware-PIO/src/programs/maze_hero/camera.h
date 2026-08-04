#pragma once

#include "maze.h"
#include "types.h"

namespace MazeHero {

struct CameraRenderPosition {
  CameraRenderPosition() : xQ8(0), yQ8(0) {}
  CameraRenderPosition(int32_t xQ8, int32_t yQ8) : xQ8(xQ8), yQ8(yQ8) {}

  int32_t xQ8;
  int32_t yQ8;
};

class Camera {
public:
  void reset(const Maze &maze, uint16_t viewWidth, uint8_t viewHeight,
             Coord hero);
  void updateTarget(const Maze &maze, uint16_t viewWidth, uint8_t viewHeight,
                    Coord hero);
  bool stepTowardTarget(unsigned long now, unsigned long durationMs);
  CameraRenderPosition renderPosition(unsigned long now) const;

  int16_t x() const { return offsetX; }
  int16_t y() const { return offsetY; }

private:
  int16_t offsetX = 0;
  int16_t offsetY = 0;
  int16_t targetX = 0;
  int16_t targetY = 0;
  int16_t renderStartX = 0;
  int16_t renderStartY = 0;
  int16_t renderEndX = 0;
  int16_t renderEndY = 0;
  unsigned long renderStartMs = 0;
  unsigned long renderDurationMs = 1;
  bool renderActive = false;

  int16_t clampX(const Maze &maze, uint16_t viewWidth, int16_t value) const;
  int16_t clampY(const Maze &maze, uint8_t viewHeight, int16_t value) const;
};

} // namespace MazeHero
