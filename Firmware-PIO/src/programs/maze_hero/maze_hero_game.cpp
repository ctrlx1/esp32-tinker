#include "maze_hero_game.h"

#include "camera.h"
#include "fog_of_war.h"
#include "hero_ai.h"
#include "maze.h"
#include "play_world.h"
#include "renderer.h"

#include <esp_system.h>

namespace MazeHero {
namespace {

constexpr unsigned long RENDER_FRAME_MS = 45UL;
constexpr uint8_t RUNNER_ZOOM_CHANCE_PER_ROLL_PERCENT = 25;
constexpr unsigned long RUNNER_ZOOM_CHANCE_INTERVAL_MS = 6000UL;
constexpr unsigned long RUNNER_ZOOM_MIN_GAP_MS = 16000UL;
constexpr uint16_t RUNNER_ZOOM_TARGET_PERCENT = 350U;
constexpr unsigned long RUNNER_ZOOM_IN_MIN_MS = 2500UL;
constexpr unsigned long RUNNER_ZOOM_IN_MAX_MS = 5500UL;
constexpr unsigned long RUNNER_ZOOM_HOLD_MIN_MS = 4000UL;
constexpr unsigned long RUNNER_ZOOM_HOLD_MAX_MS = 14000UL;
constexpr unsigned long RUNNER_ZOOM_OUT_MIN_MS = 2500UL;
constexpr unsigned long RUNNER_ZOOM_OUT_MAX_MS = 5500UL;
constexpr unsigned long HERO_SPEED_TRANSITION_MIN_MS = 4000UL;
constexpr unsigned long HERO_SPEED_TRANSITION_MAX_MS = 12000UL;

enum class GameState : uint8_t { IntroAnimation, Playing, VictoryAnimation };
enum class MovementPhase : uint8_t { IdleAtCell, CrossingPassage };

struct HeroRenderPosition {
  HeroRenderPosition() : xQ8(0), yQ8(0) {}
  HeroRenderPosition(int32_t xQ8, int32_t yQ8) : xQ8(xQ8), yQ8(yQ8) {}

  int32_t xQ8;
  int32_t yQ8;
};

struct RunnerZoomState {
  bool active = false;
  unsigned long startMs = 0;
  unsigned long lastRollMs = 0;
  unsigned long lastEndMs = 0;
  unsigned long zoomInMs = 0;
  unsigned long holdMs = 0;
  unsigned long zoomOutMs = 0;
};

struct HeroSpeedState {
  unsigned long startDelayMs = 1;
  unsigned long targetDelayMs = 1;
  unsigned long startMs = 0;
  unsigned long durationMs = 1;
};

struct Game {
  Maze maze;
  FogOfWar fog;
  Camera camera;
  Renderer renderer;
  HeroAi ai;
  Coord hero;
  Coord pendingHero;
  int16_t heroWorldX = 0;
  int16_t heroWorldY = 0;
  int16_t heroRenderStartWorldX = 0;
  int16_t heroRenderStartWorldY = 0;
  int16_t heroRenderEndWorldX = 0;
  int16_t heroRenderEndWorldY = 0;
  unsigned long heroRenderStartMs = 0;
  unsigned long heroRenderDurationMs = 1;
  bool heroRenderActive = false;
  GameState state = GameState::Playing;
  MovementPhase movementPhase = MovementPhase::IdleAtCell;
  bool seeded = false;
  uint8_t frame = 0;
  unsigned long lastMoveMs = 0;
  unsigned long moveDelayMs = 1;
  unsigned long lastRenderMs = 0;
  unsigned long introStartMs = 0;
  unsigned long victoryStartMs = 0;
  RunnerZoomState runnerZoom;
  HeroSpeedState heroSpeed;
  ZoomView victoryStartView;
};

Game game;

bool heroAtPendingCellCenter() {
  return game.heroWorldX == cellCenterX(game.pendingHero) &&
         game.heroWorldY == cellCenterY(game.pendingHero);
}

int32_t worldQ8(int16_t value) { return (int32_t)value * 256L; }

uint8_t progressQ8(unsigned long elapsedMs, unsigned long durationMs) {
  if (durationMs == 0) {
    return 255;
  }
  if (elapsedMs >= durationMs) {
    return 255;
  }
  return (uint8_t)((elapsedMs * 255UL) / durationMs);
}

int32_t lerpWorldQ8(int16_t from, int16_t to, uint8_t progress) {
  return worldQ8(from) + ((int32_t)(to - from) * 256L * progress) / 255L;
}

bool heroRenderAnimationActive(unsigned long now) {
  return game.heroRenderActive &&
         now - game.heroRenderStartMs < game.heroRenderDurationMs;
}

HeroRenderPosition currentHeroRenderPosition(unsigned long now) {
  if (!game.heroRenderActive) {
    return {worldQ8(game.heroWorldX), worldQ8(game.heroWorldY)};
  }

  unsigned long elapsedMs = now - game.heroRenderStartMs;
  if (elapsedMs >= game.heroRenderDurationMs) {
    game.heroRenderActive = false;
    return {worldQ8(game.heroRenderEndWorldX),
            worldQ8(game.heroRenderEndWorldY)};
  }

  uint8_t progress = progressQ8(elapsedMs, game.heroRenderDurationMs);
  return {lerpWorldQ8(game.heroRenderStartWorldX, game.heroRenderEndWorldX,
                      progress),
          lerpWorldQ8(game.heroRenderStartWorldY, game.heroRenderEndWorldY,
                      progress)};
}

void resetMoveTimer(unsigned long now, unsigned long moveDelayMs) {
  game.lastMoveMs = now;
  game.moveDelayMs = moveDelayMs > 0 ? moveDelayMs : 1UL;
}

void stepHeroTowardPendingCell(unsigned long now, unsigned long moveDelayMs) {
  int16_t startWorldX = game.heroWorldX;
  int16_t startWorldY = game.heroWorldY;
  int16_t endWorldX =
      approachOnePixel(startWorldX, cellCenterX(game.pendingHero));
  int16_t endWorldY =
      approachOnePixel(startWorldY, cellCenterY(game.pendingHero));

  game.heroWorldX = endWorldX;
  game.heroWorldY = endWorldY;
  game.heroRenderStartWorldX = startWorldX;
  game.heroRenderStartWorldY = startWorldY;
  game.heroRenderEndWorldX = endWorldX;
  game.heroRenderEndWorldY = endWorldY;
  game.heroRenderStartMs = now;
  game.heroRenderDurationMs = moveDelayMs > 0 ? moveDelayMs : 1UL;
  game.heroRenderActive = startWorldX != endWorldX || startWorldY != endWorldY;
}

void completeCellArrival() {
  game.hero = game.pendingHero;
  game.fog.markHeroVisited(game.maze, game.hero);
  game.fog.revealFrom(game.maze, game.hero);
  game.movementPhase = MovementPhase::IdleAtCell;
}

unsigned long runnerZoomTotalMs() {
  return game.runnerZoom.zoomInMs + game.runnerZoom.holdMs +
         game.runnerZoom.zoomOutMs;
}

unsigned long randomRangeInclusive(unsigned long minValue,
                                   unsigned long maxValue) {
  if (maxValue < minValue) {
    maxValue = minValue;
  }
  return random(minValue, maxValue + 1);
}

struct HeroSpeedBounds {
  unsigned long minDelayMs;
  unsigned long maxDelayMs;
};

HeroSpeedBounds heroSpeedBounds(const ProgramConfig &cfg) {
  unsigned long minDelayMs =
      cfg.mazeHeroMinSpeedMs > 0 ? cfg.mazeHeroMinSpeedMs : 1UL;
  unsigned long maxDelayMs = cfg.mazeHeroMaxSpeedMs >= minDelayMs
                                 ? cfg.mazeHeroMaxSpeedMs
                                 : minDelayMs;
  return {minDelayMs, maxDelayMs};
}

unsigned long clampHeroSpeedDelay(unsigned long delayMs,
                                  const HeroSpeedBounds &bounds) {
  if (delayMs < bounds.minDelayMs) {
    return bounds.minDelayMs;
  }
  if (delayMs > bounds.maxDelayMs) {
    return bounds.maxDelayMs;
  }
  return delayMs > 0 ? delayMs : 1UL;
}

unsigned long randomHeroSpeedDelayMs(const ProgramConfig &cfg) {
  HeroSpeedBounds bounds = heroSpeedBounds(cfg);
  return randomRangeInclusive(bounds.minDelayMs, bounds.maxDelayMs);
}

unsigned long lerpHeroSpeedDelayMs(unsigned long fromDelayMs,
                                   unsigned long toDelayMs,
                                   uint8_t progress) {
  if (fromDelayMs == toDelayMs) {
    return fromDelayMs;
  }

  uint64_t delta = fromDelayMs > toDelayMs ? fromDelayMs - toDelayMs
                                           : toDelayMs - fromDelayMs;
  unsigned long offset = (unsigned long)((delta * progress) / 255ULL);
  return fromDelayMs > toDelayMs ? fromDelayMs - offset
                                 : fromDelayMs + offset;
}

unsigned long currentHeroSpeedDelayMs(unsigned long now) {
  unsigned long elapsedMs = now - game.heroSpeed.startMs;
  if (elapsedMs >= game.heroSpeed.durationMs) {
    return game.heroSpeed.targetDelayMs;
  }

  uint8_t progress = progressQ8(elapsedMs, game.heroSpeed.durationMs);
  return lerpHeroSpeedDelayMs(game.heroSpeed.startDelayMs,
                              game.heroSpeed.targetDelayMs, progress);
}

void startHeroSpeedTransition(const ProgramConfig &cfg, unsigned long now,
                              unsigned long startDelayMs) {
  HeroSpeedBounds bounds = heroSpeedBounds(cfg);
  game.heroSpeed.startDelayMs = clampHeroSpeedDelay(startDelayMs, bounds);
  game.heroSpeed.targetDelayMs = randomHeroSpeedDelayMs(cfg);
  game.heroSpeed.startMs = now;
  game.heroSpeed.durationMs = randomRangeInclusive(HERO_SPEED_TRANSITION_MIN_MS,
                                                   HERO_SPEED_TRANSITION_MAX_MS);
}

void resetHeroSpeed(const ProgramConfig &cfg, unsigned long now) {
  unsigned long startDelayMs = randomHeroSpeedDelayMs(cfg);
  game.heroSpeed.startDelayMs = startDelayMs;
  game.heroSpeed.targetDelayMs = startDelayMs;
  game.heroSpeed.startMs = now;
  game.heroSpeed.durationMs = 1;
  startHeroSpeedTransition(cfg, now, startDelayMs);
}

unsigned long nextHeroMoveDelayMs(const ProgramConfig &cfg,
                                  unsigned long now) {
  unsigned long delayMs = currentHeroSpeedDelayMs(now);
  if (now - game.heroSpeed.startMs >= game.heroSpeed.durationMs) {
    startHeroSpeedTransition(cfg, now, delayMs);
    delayMs = currentHeroSpeedDelayMs(now);
  }
  return delayMs > 0 ? delayMs : 1UL;
}

PlayZoomFrame currentRunnerZoomFrame(unsigned long now) {
  PlayZoomFrame frame;
  if (!game.runnerZoom.active) {
    return frame;
  }

  frame.active = true;
  frame.elapsedMs = now - game.runnerZoom.startMs;
  frame.zoomInMs = game.runnerZoom.zoomInMs;
  frame.holdMs = game.runnerZoom.holdMs;
  frame.zoomOutMs = game.runnerZoom.zoomOutMs;
  frame.targetZoomPercent = RUNNER_ZOOM_TARGET_PERCENT;
  return frame;
}

void resetRunnerZoom(unsigned long now) {
  game.runnerZoom.active = false;
  game.runnerZoom.startMs = 0;
  game.runnerZoom.lastRollMs = now;
  game.runnerZoom.lastEndMs = now;
  game.runnerZoom.zoomInMs = 0;
  game.runnerZoom.holdMs = 0;
  game.runnerZoom.zoomOutMs = 0;
}

void updateRunnerZoom(unsigned long now) {
  if (!game.runnerZoom.active) {
    return;
  }

  if (now - game.runnerZoom.startMs >= runnerZoomTotalMs()) {
    game.runnerZoom.active = false;
    game.runnerZoom.lastEndMs = now;
    game.runnerZoom.lastRollMs = now;
  }
}

void maybeStartRunnerZoom(unsigned long now) {
  if (game.runnerZoom.active || sameCoord(game.hero, game.maze.finish())) {
    return;
  }
  if (now - game.runnerZoom.lastEndMs < RUNNER_ZOOM_MIN_GAP_MS) {
    return;
  }
  if (now - game.runnerZoom.lastRollMs < RUNNER_ZOOM_CHANCE_INTERVAL_MS) {
    return;
  }

  game.runnerZoom.lastRollMs = now;
  if (random(100) >= RUNNER_ZOOM_CHANCE_PER_ROLL_PERCENT) {
    return;
  }

  game.runnerZoom.active = true;
  game.runnerZoom.startMs = now;
  game.runnerZoom.zoomInMs =
      randomRangeInclusive(RUNNER_ZOOM_IN_MIN_MS, RUNNER_ZOOM_IN_MAX_MS);
  game.runnerZoom.holdMs =
      randomRangeInclusive(RUNNER_ZOOM_HOLD_MIN_MS, RUNNER_ZOOM_HOLD_MAX_MS);
  game.runnerZoom.zoomOutMs =
      randomRangeInclusive(RUNNER_ZOOM_OUT_MIN_MS, RUNNER_ZOOM_OUT_MAX_MS);
}

void startVictory(unsigned long now) {
  PlayZoomFrame zoomFrame = currentRunnerZoomFrame(now);
  HeroRenderPosition heroRender = currentHeroRenderPosition(now);
  CameraRenderPosition cameraRender = game.camera.renderPosition(now);
  game.victoryStartView = game.renderer.playZoomView(
      game.maze, cameraRender, heroRender.xQ8, heroRender.yQ8, zoomFrame);
  resetRunnerZoom(now);
  game.state = GameState::VictoryAnimation;
  game.victoryStartMs = now;
  game.lastRenderMs = 0;
}

void startNewMaze(const ProgramConfig &cfg) {
  unsigned long now = millis();
  game.maze.generate(cfg.mazeMinWidth, cfg.mazeMaxWidth, cfg.mazeMinHeight,
                     cfg.mazeMaxHeight);
  game.hero = game.maze.start();
  game.pendingHero = game.hero;
  game.heroWorldX = cellCenterX(game.hero);
  game.heroWorldY = cellCenterY(game.hero);
  game.heroRenderStartWorldX = game.heroWorldX;
  game.heroRenderStartWorldY = game.heroWorldY;
  game.heroRenderEndWorldX = game.heroWorldX;
  game.heroRenderEndWorldY = game.heroWorldY;
  game.heroRenderStartMs = now;
  game.heroRenderDurationMs = 1;
  game.heroRenderActive = false;
  game.ai.reset();
  game.fog.reset(game.maze);
  game.fog.markHeroVisited(game.maze, game.hero);
  game.fog.revealFrom(game.maze, game.hero);
  game.camera.reset(game.maze, game.renderer.viewWidth(),
                    game.renderer.viewHeight(), game.hero);
  resetRunnerZoom(now);
  game.victoryStartView = game.renderer.playZoomView(
      game.maze, game.camera.renderPosition(now), worldQ8(game.heroWorldX),
      worldQ8(game.heroWorldY), currentRunnerZoomFrame(now));
  game.state = GameState::IntroAnimation;
  game.movementPhase = MovementPhase::IdleAtCell;
  game.frame = 0;
  game.lastMoveMs = now;
  resetHeroSpeed(cfg, now);
  game.moveDelayMs = currentHeroSpeedDelayMs(now);
  game.lastRenderMs = now;
  game.introStartMs = now;
  game.victoryStartMs = 0;
  game.renderer.renderIntro(game.maze, game.fog, game.camera, game.hero, 0,
                            game.frame++);
}

void tickIntro(unsigned long now) {
  unsigned long elapsedMs = now - game.introStartMs;
  if (elapsedMs >= INTRO_TOTAL_MS) {
    game.state = GameState::Playing;
    game.lastMoveMs = now;
    game.lastRenderMs = 0;
    return;
  }

  if (now - game.lastRenderMs >= RENDER_FRAME_MS) {
    game.lastRenderMs = now;
    game.renderer.renderIntro(game.maze, game.fog, game.camera, game.hero,
                              elapsedMs, game.frame++);
  }
}

void tickPlaying(const ProgramConfig &cfg, unsigned long now) {
  updateRunnerZoom(now);

  bool waitingForVictory =
      game.movementPhase == MovementPhase::IdleAtCell &&
      sameCoord(game.hero, game.maze.finish());
  if (waitingForVictory && !heroRenderAnimationActive(now)) {
    startVictory(now);
    return;
  }

  maybeStartRunnerZoom(now);

  if (!waitingForVictory && now - game.lastMoveMs >= game.moveDelayMs) {
    if (game.movementPhase == MovementPhase::CrossingPassage) {
      unsigned long nextMoveDelayMs = nextHeroMoveDelayMs(cfg, now);
      stepHeroTowardPendingCell(now, nextMoveDelayMs);
      game.camera.stepTowardTarget(now, nextMoveDelayMs);
      resetMoveTimer(now, nextMoveDelayMs);
      if (heroAtPendingCellCenter()) {
        completeCellArrival();
      }
    } else {
      Coord next;
      unsigned long decisionDelayMs = game.moveDelayMs;
      if (game.ai.chooseNextStep(game.maze, game.fog, game.hero, now,
                                 decisionDelayMs, next)) {
        game.pendingHero = next;
        game.movementPhase = MovementPhase::CrossingPassage;
        unsigned long nextMoveDelayMs = nextHeroMoveDelayMs(cfg, now);
        // Follow the committed destination while AI/fog remain cell-boundary
        // driven until the visible hero reaches the next cell center.
        game.camera.updateTarget(game.maze, game.renderer.viewWidth(),
                                 game.renderer.viewHeight(), game.pendingHero);
        stepHeroTowardPendingCell(now, nextMoveDelayMs);
        game.camera.stepTowardTarget(now, nextMoveDelayMs);
        resetMoveTimer(now, nextMoveDelayMs);
      }
    }
  }

  if (now - game.lastRenderMs >= RENDER_FRAME_MS) {
    game.lastRenderMs = now;
    PlayZoomFrame zoomFrame = currentRunnerZoomFrame(now);
    HeroRenderPosition heroRender = currentHeroRenderPosition(now);
    CameraRenderPosition cameraRender = game.camera.renderPosition(now);
    game.renderer.renderPlaying(game.maze, game.fog, cameraRender,
                                heroRender.xQ8, heroRender.yQ8, zoomFrame,
                                game.frame++);
  }
}

void tickVictory(const ProgramConfig &cfg, unsigned long now) {
  unsigned long elapsedMs = now - game.victoryStartMs;
  if (elapsedMs >= VICTORY_TOTAL_MS) {
    startNewMaze(cfg);
    return;
  }

  if (now - game.lastRenderMs >= RENDER_FRAME_MS) {
    game.lastRenderMs = now;
    game.renderer.renderVictory(game.maze, game.fog, game.victoryStartView,
                                game.hero, elapsedMs, game.frame++);
  }
}

} // namespace

void start(const ProgramConfig &cfg) {
  if (!game.seeded) {
    randomSeed(esp_random());
    game.seeded = true;
  }
  Display.setIntensity(cfg.brightness > 15 ? 15 : cfg.brightness);
  startNewMaze(cfg);
}

void tick(const ProgramConfig &cfg) {
  unsigned long now = millis();

  switch (game.state) {
  case GameState::IntroAnimation:
    tickIntro(now);
    break;
  case GameState::Playing:
    tickPlaying(cfg, now);
    break;
  case GameState::VictoryAnimation:
    tickVictory(cfg, now);
    break;
  }
}

} // namespace MazeHero
