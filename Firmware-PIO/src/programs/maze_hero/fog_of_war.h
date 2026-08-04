#pragma once

#include "maze.h"
#include "types.h"

namespace MazeHero {

class FogOfWar {
public:
  void reset(const Maze &maze);
  void revealFrom(const Maze &maze, Coord hero);

  bool isDiscovered(const Maze &maze, Coord coord) const;
  bool wasHeroVisited(const Maze &maze, Coord coord) const;
  void markHeroVisited(const Maze &maze, Coord coord);

private:
  bool discovered[MAX_MAZE_CELLS];
  bool heroVisited[MAX_MAZE_CELLS];
};

} // namespace MazeHero
