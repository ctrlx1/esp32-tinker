#pragma once

#include "fog_of_war.h"
#include "maze.h"
#include "types.h"

namespace MazeHero {

class HeroAi {
public:
  void reset();
  bool chooseNextStep(const Maze &maze, const FogOfWar &fog, Coord hero,
                      unsigned long now, unsigned long heroSpeedMs,
                      Coord &next);

private:
  bool seen[MAX_MAZE_CELLS];
  Coord queue[MAX_MAZE_CELLS];
  Coord firstStep[MAX_MAZE_CELLS];
  Coord previousStep[MAX_MAZE_CELLS];
  Coord previousHero;
  Coord pauseCoord;
  Coord pauseConsumedCoord;
  bool hasPreviousHero = false;
  bool pauseActive = false;
  bool pauseConsumed = false;
  unsigned long pauseUntilMs = 0;

  bool chooseBestAdjacentUnvisited(const Maze &maze, const FogOfWar &fog,
                                   Coord hero, Coord &next) const;
  bool chooseFallbackStep(const Maze &maze, Coord hero, Coord &next) const;
  bool isDeadEndFrom(const Maze &maze, Coord current, Coord previous) const;
  bool shouldPauseAtDecision(const Maze &maze, Coord hero) const;
  bool maybePauseAtDecision(const Maze &maze, Coord hero, unsigned long now,
                            unsigned long heroSpeedMs);
  void recordMove(Coord from, Coord to);
};

} // namespace MazeHero
