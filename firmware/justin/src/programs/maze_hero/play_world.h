#pragma once

#include "maze.h"
#include "types.h"

namespace MazeHero {

inline int16_t approachOnePixel(int16_t current, int16_t target) {
  if (current < target) {
    return current + 1;
  }
  if (current > target) {
    return current - 1;
  }
  return current;
}

inline int16_t mazePixelWidth(const Maze &maze) {
  return (int16_t)maze.width() * 2 + 1;
}

inline int16_t mazePixelHeight(const Maze &maze) {
  return (int16_t)maze.height() * 2 + 1;
}

inline int16_t cellCenterX(Coord coord) { return (int16_t)coord.x * 2 + 1; }

inline int16_t cellCenterY(Coord coord) { return (int16_t)coord.y * 2 + 1; }

} // namespace MazeHero
