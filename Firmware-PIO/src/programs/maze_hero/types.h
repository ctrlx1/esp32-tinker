#pragma once

#include <Arduino.h>

namespace MazeHero {

constexpr uint8_t MIN_MAZE_DIMENSION = 4;
constexpr uint8_t MAX_MAZE_WIDTH = 80;
constexpr uint8_t MAX_MAZE_HEIGHT = 48;
constexpr uint16_t MAX_MAZE_CELLS = MAX_MAZE_WIDTH * MAX_MAZE_HEIGHT;

constexpr uint8_t DISPLAY_HEIGHT = 8;

struct Coord {
  uint8_t x;
  uint8_t y;

  Coord() : x(0), y(0) {}
  Coord(uint8_t coordX, uint8_t coordY) : x(coordX), y(coordY) {}
};

inline bool sameCoord(Coord a, Coord b) { return a.x == b.x && a.y == b.y; }

enum class Direction : uint8_t { North = 0, East = 1, South = 2, West = 3 };

inline uint8_t directionMask(Direction dir) { return 1U << (uint8_t)dir; }

inline Direction opposite(Direction dir) {
  return (Direction)(((uint8_t)dir + 2U) & 0x03U);
}

inline int8_t directionDx(Direction dir) {
  if (dir == Direction::East) {
    return 1;
  }
  if (dir == Direction::West) {
    return -1;
  }
  return 0;
}

inline int8_t directionDy(Direction dir) {
  if (dir == Direction::South) {
    return 1;
  }
  if (dir == Direction::North) {
    return -1;
  }
  return 0;
}

} // namespace MazeHero
