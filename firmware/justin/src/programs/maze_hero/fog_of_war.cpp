#include "fog_of_war.h"

namespace MazeHero {

void FogOfWar::reset(const Maze &maze) {
  uint16_t cellCount = maze.width() * maze.height();
  for (uint16_t i = 0; i < cellCount; i++) {
    discovered[i] = false;
    heroVisited[i] = false;
  }
}

void FogOfWar::revealFrom(const Maze &maze, Coord hero) {
  if (!maze.contains(hero)) {
    return;
  }

  discovered[maze.index(hero)] = true;

  for (uint8_t d = 0; d < 4; d++) {
    Direction dir = (Direction)d;
    Coord current = hero;
    Coord next;
    while (maze.canMove(current, dir, next)) {
      discovered[maze.index(next)] = true;
      current = next;
    }
  }
}

bool FogOfWar::isDiscovered(const Maze &maze, Coord coord) const {
  return maze.contains(coord) && discovered[maze.index(coord)];
}

bool FogOfWar::wasHeroVisited(const Maze &maze, Coord coord) const {
  return maze.contains(coord) && heroVisited[maze.index(coord)];
}

void FogOfWar::markHeroVisited(const Maze &maze, Coord coord) {
  if (maze.contains(coord)) {
    heroVisited[maze.index(coord)] = true;
  }
}

} // namespace MazeHero
