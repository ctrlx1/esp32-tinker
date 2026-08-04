#include "hero_ai.h"

namespace MazeHero {
namespace {

constexpr uint8_t DECISION_PAUSE_CHANCE_NUMERATOR = 1;
constexpr uint8_t DECISION_PAUSE_CHANCE_DENOMINATOR = 3;
constexpr uint8_t DECISION_PAUSE_MIN_SPEED_PERCENT = 50;
constexpr uint8_t DECISION_PAUSE_MAX_SPEED_PERCENT = 150;

bool timeReached(unsigned long now, unsigned long target) {
  return (int32_t)(now - target) >= 0;
}

} // namespace

void HeroAi::reset() {
  hasPreviousHero = false;
  pauseActive = false;
  pauseConsumed = false;
  pauseUntilMs = 0;
}

bool HeroAi::chooseNextStep(const Maze &maze, const FogOfWar &fog, Coord hero,
                            unsigned long now, unsigned long heroSpeedMs,
                            Coord &next) {
  if (pauseActive) {
    if (sameCoord(hero, pauseCoord) && !timeReached(now, pauseUntilMs)) {
      return false;
    }
    pauseActive = false;
    pauseConsumed = true;
    pauseConsumedCoord = hero;
  }

  Coord finish = maze.finish();
  bool finishKnown = fog.isDiscovered(maze, finish);

  if (!finishKnown && maybePauseAtDecision(maze, hero, now, heroSpeedMs)) {
    return false;
  }

  if (!finishKnown && chooseBestAdjacentUnvisited(maze, fog, hero, next)) {
    recordMove(hero, next);
    return true;
  }

  uint16_t cellCount = maze.width() * maze.height();
  for (uint16_t i = 0; i < cellCount; i++) {
    seen[i] = false;
  }

  uint16_t head = 0;
  uint16_t tail = 0;
  queue[tail++] = hero;
  seen[maze.index(hero)] = true;
  firstStep[maze.index(hero)] = hero;
  previousStep[maze.index(hero)] = hero;

  while (head < tail) {
    Coord current = queue[head++];
    bool isHero = sameCoord(current, hero);
    bool isTarget = !isHero && fog.isDiscovered(maze, current) &&
                    ((finishKnown && sameCoord(current, finish)) ||
                     (!finishKnown && !fog.wasHeroVisited(maze, current) &&
                      !isDeadEndFrom(maze, current,
                                     previousStep[maze.index(current)])));
    if (isTarget) {
      next = firstStep[maze.index(current)];
      recordMove(hero, next);
      return true;
    }

    for (uint8_t d = 0; d < 4; d++) {
      Coord candidate;
      if (!maze.canMove(current, (Direction)d, candidate) ||
          !fog.isDiscovered(maze, candidate)) {
        continue;
      }

      uint16_t candidateIndex = maze.index(candidate);
      if (seen[candidateIndex]) {
        continue;
      }

      seen[candidateIndex] = true;
      firstStep[candidateIndex] =
          isHero ? candidate : firstStep[maze.index(current)];
      previousStep[candidateIndex] = current;
      queue[tail++] = candidate;
    }
  }

  if (chooseFallbackStep(maze, hero, next)) {
    recordMove(hero, next);
    return true;
  }
  return false;
}

bool HeroAi::chooseBestAdjacentUnvisited(const Maze &maze, const FogOfWar &fog,
                                         Coord hero, Coord &next) const {
  for (uint8_t d = 0; d < 4; d++) {
    Coord candidate;
    if (!maze.canMove(hero, (Direction)d, candidate) ||
        !fog.isDiscovered(maze, candidate) ||
        fog.wasHeroVisited(maze, candidate) ||
        isDeadEndFrom(maze, candidate, hero)) {
      continue;
    }

    next = candidate;
    return true;
  }

  return false;
}

bool HeroAi::chooseFallbackStep(const Maze &maze, Coord hero, Coord &next) const {
  for (uint8_t d = 0; d < 4; d++) {
    if (maze.canMove(hero, (Direction)d, next)) {
      return true;
    }
  }
  return false;
}

bool HeroAi::isDeadEndFrom(const Maze &maze, Coord current,
                           Coord previous) const {
  for (uint8_t d = 0; d < 4; d++) {
    Coord candidate;
    if (!maze.canMove(current, (Direction)d, candidate) ||
        sameCoord(candidate, previous)) {
      continue;
    }

    return false;
  }

  return true;
}

bool HeroAi::shouldPauseAtDecision(const Maze &maze, Coord hero) const {
  uint8_t onwardExitCount = 0;
  for (uint8_t d = 0; d < 4; d++) {
    Coord candidate;
    if (!maze.canMove(hero, (Direction)d, candidate)) {
      continue;
    }
    if (hasPreviousHero && sameCoord(candidate, previousHero)) {
      continue;
    }

    onwardExitCount++;
  }

  return onwardExitCount > 1;
}

bool HeroAi::maybePauseAtDecision(const Maze &maze, Coord hero,
                                  unsigned long now,
                                  unsigned long heroSpeedMs) {
  if (pauseConsumed && sameCoord(hero, pauseConsumedCoord)) {
    return false;
  }
  if (!shouldPauseAtDecision(maze, hero)) {
    return false;
  }
  if (random(0, DECISION_PAUSE_CHANCE_DENOMINATOR) >=
      DECISION_PAUSE_CHANCE_NUMERATOR) {
    return false;
  }

  unsigned long safeHeroSpeedMs = heroSpeedMs > 0 ? heroSpeedMs : 1UL;
  unsigned long minPauseMs =
      (safeHeroSpeedMs * DECISION_PAUSE_MIN_SPEED_PERCENT) / 100UL;
  unsigned long maxPauseMs =
      (safeHeroSpeedMs * DECISION_PAUSE_MAX_SPEED_PERCENT) / 100UL;
  if (minPauseMs == 0) {
    minPauseMs = 1;
  }
  pauseActive = true;
  pauseCoord = hero;
  pauseUntilMs = now + random(minPauseMs, maxPauseMs + 1);
  return true;
}

void HeroAi::recordMove(Coord from, Coord to) {
  (void)to;
  previousHero = from;
  hasPreviousHero = true;
  pauseConsumed = false;
}

} // namespace MazeHero
