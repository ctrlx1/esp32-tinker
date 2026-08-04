#include "maze.h"

#include <Arduino.h>

namespace MazeHero {
namespace {

uint8_t clampDimension(unsigned int value, uint8_t fallback, uint8_t maxValue) {
  if (value < MIN_MAZE_DIMENSION) {
    return fallback;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return (uint8_t)value;
}

} // namespace

void Maze::generate(unsigned int minWidth, unsigned int maxWidth,
                    unsigned int minHeight, unsigned int maxHeight) {
  uint8_t safeMinWidth =
      clampDimension(minWidth, MIN_MAZE_DIMENSION, MAX_MAZE_WIDTH);
  uint8_t safeMaxWidth = clampDimension(maxWidth, safeMinWidth, MAX_MAZE_WIDTH);
  if (safeMaxWidth < safeMinWidth) {
    safeMaxWidth = safeMinWidth;
  }

  uint8_t safeMinHeight =
      clampDimension(minHeight, MIN_MAZE_DIMENSION, MAX_MAZE_HEIGHT);
  uint8_t safeMaxHeight =
      clampDimension(maxHeight, safeMinHeight, MAX_MAZE_HEIGHT);
  if (safeMaxHeight < safeMinHeight) {
    safeMaxHeight = safeMinHeight;
  }

  mazeWidth = (uint8_t)random(safeMinWidth, safeMaxWidth + 1);
  mazeHeight = (uint8_t)random(safeMinHeight, safeMaxHeight + 1);

  resetCells();
  carveMaze();
  placeStartAndFinish();
}

bool Maze::contains(Coord coord) const {
  return coord.x < mazeWidth && coord.y < mazeHeight;
}

bool Maze::hasWall(Coord coord, Direction dir) const {
  if (!contains(coord)) {
    return true;
  }
  return (cells[index(coord)].walls & directionMask(dir)) != 0;
}

bool Maze::neighbor(Coord coord, Direction dir, Coord &out) const {
  int16_t nx = coord.x + directionDx(dir);
  int16_t ny = coord.y + directionDy(dir);
  if (nx < 0 || ny < 0 || nx >= mazeWidth || ny >= mazeHeight) {
    return false;
  }

  out.x = (uint8_t)nx;
  out.y = (uint8_t)ny;
  return true;
}

bool Maze::canMove(Coord coord, Direction dir, Coord &out) const {
  return neighbor(coord, dir, out) && !hasWall(coord, dir);
}

uint16_t Maze::index(Coord coord) const { return coord.y * mazeWidth + coord.x; }

void Maze::resetCells() {
  uint16_t cellCount = mazeWidth * mazeHeight;
  for (uint16_t i = 0; i < cellCount; i++) {
    cells[i].walls = 0x0F;
    visited[i] = false;
  }
}

void Maze::clearWall(Coord coord, Direction dir) {
  cells[index(coord)].walls &= ~directionMask(dir);
}

void Maze::carveMaze() {
  uint16_t stackSize = 0;
  Coord current = {(uint8_t)random(0, mazeWidth), (uint8_t)random(0, mazeHeight)};
  visited[index(current)] = true;
  stack[stackSize++] = current;

  while (stackSize > 0) {
    current = stack[stackSize - 1];

    Direction options[4];
    uint8_t optionCount = 0;
    for (uint8_t d = 0; d < 4; d++) {
      Direction dir = (Direction)d;
      Coord next;
      if (neighbor(current, dir, next) && !visited[index(next)]) {
        options[optionCount++] = dir;
      }
    }

    if (optionCount == 0) {
      stackSize--;
      continue;
    }

    Direction chosen = options[random(0, optionCount)];
    Coord next;
    if (!neighbor(current, chosen, next)) {
      continue;
    }

    clearWall(current, chosen);
    clearWall(next, opposite(chosen));
    visited[index(next)] = true;
    stack[stackSize++] = next;
  }
}

Coord Maze::randomEdgeCell() const {
  uint8_t edge = (uint8_t)random(0, 4);
  switch (edge) {
  case 0:
    return {(uint8_t)random(0, mazeWidth), 0};
  case 1:
    return {(uint8_t)(mazeWidth - 1), (uint8_t)random(0, mazeHeight)};
  case 2:
    return {(uint8_t)random(0, mazeWidth), (uint8_t)(mazeHeight - 1)};
  default:
    return {0, (uint8_t)random(0, mazeHeight)};
  }
}

bool Maze::isEdgeCell(Coord coord) const {
  return coord.x == 0 || coord.y == 0 || coord.x == mazeWidth - 1 ||
         coord.y == mazeHeight - 1;
}

Direction Maze::outwardEdgeDirection(Coord coord) const {
  if (coord.y == 0) {
    return Direction::North;
  }
  if (coord.x == mazeWidth - 1) {
    return Direction::East;
  }
  if (coord.y == mazeHeight - 1) {
    return Direction::South;
  }
  return Direction::West;
}

void Maze::openEdge(Coord coord) {
  if (isEdgeCell(coord)) {
    clearWall(coord, outwardEdgeDirection(coord));
  }
}

void Maze::placeStartAndFinish() {
  startCoord = randomEdgeCell();

  uint16_t cellCount = mazeWidth * mazeHeight;
  for (uint16_t i = 0; i < cellCount; i++) {
    distances[i] = -1;
  }

  uint16_t head = 0;
  uint16_t tail = 0;
  queue[tail++] = startCoord;
  distances[index(startCoord)] = 0;

  while (head < tail) {
    Coord current = queue[head++];
    int16_t currentDistance = distances[index(current)];
    for (uint8_t d = 0; d < 4; d++) {
      Coord next;
      if (!canMove(current, (Direction)d, next)) {
        continue;
      }
      uint16_t nextIndex = index(next);
      if (distances[nextIndex] >= 0) {
        continue;
      }
      distances[nextIndex] = currentDistance + 1;
      queue[tail++] = next;
    }
  }

  finishCoord = startCoord;
  int16_t bestDistance = -1;
  for (uint8_t y = 0; y < mazeHeight; y++) {
    for (uint8_t x = 0; x < mazeWidth; x++) {
      Coord candidate = {x, y};
      int16_t distance = distances[index(candidate)];
      if (isEdgeCell(candidate) && !sameCoord(candidate, startCoord) &&
          distance > bestDistance) {
        bestDistance = distance;
        finishCoord = candidate;
      }
    }
  }

  openEdge(startCoord);
  openEdge(finishCoord);
}

} // namespace MazeHero
