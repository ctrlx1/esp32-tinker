#include "weather_watch.h"

#include <Arduino.h>
#include <MD_MAX72xx.h>

namespace {
constexpr uint8_t DISPLAY_HEIGHT = 8;
constexpr uint8_t DISPLAY_WIDTH = 32;
constexpr unsigned long FRAME_MS = 80UL;
constexpr unsigned long SCENE_MS = 7000UL;
constexpr uint8_t MAX_DROPS = 18;
constexpr uint8_t MAX_FLAKES = 14;
constexpr uint8_t MAX_BOLTS = 2;
constexpr uint8_t FLASH_FRAMES = 2;
constexpr uint8_t BOLT_HOLD_FRAMES = 4;

enum class Scene : uint8_t { Sunny, Cloudy, Rainy, Stormy, Snowy, Count };

struct Drop {
  int8_t x;
  int8_t y;
  int8_t speed;
  bool active;
};

struct Bolt {
  int8_t originX;
  int8_t variant;
  int8_t life;
  bool active;
};

struct State {
  Scene scene = Scene::Sunny;
  unsigned long sceneStartMs = 0;
  unsigned long lastFrameMs = 0;
  uint16_t frame = 0;
  uint8_t brightness = 0;
  int8_t flashFrames = 0;
  int8_t nextStrikeIn = 0;
  Bolt bolts[MAX_BOLTS];
  Drop drops[MAX_DROPS];
  Drop flakes[MAX_FLAKES];
};

State state;

MD_MAX72XX *matrix() { return Display.getGraphicObject(); }

uint8_t sanitizedBrightness(uint8_t brightness) {
  return brightness > 15 ? 15 : brightness;
}

void beginFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  matrix()->clear();
}

void endFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix()->update();
}

void setPixel(int16_t row, int16_t col, bool on = true) {
  if (row < 0 || row >= DISPLAY_HEIGHT || col < 0 || col >= DISPLAY_WIDTH) {
    return;
  }
  matrix()->setPoint(row, col, on);
}

void drawSun(int16_t cx, int16_t cy, uint16_t frame) {
  for (int16_t dy = -1; dy <= 1; dy++) {
    for (int16_t dx = -1; dx <= 1; dx++) {
      if (abs(dx) + abs(dy) <= 2) {
        setPixel(cy + dy, cx + dx);
      }
    }
  }

  const int8_t rays[8][2] = {{0, -3}, {2, -2}, {3, 0},  {2, 2},
                             {0, 3},  {-2, 2}, {-3, 0}, {-2, -2}};
  for (uint8_t i = 0; i < 8; i++) {
    if (((frame / 2) + i) % 2 == 0) {
      setPixel(cy + rays[i][1], cx + rays[i][0]);
    }
  }
}

void drawCloud(int16_t baseX, int16_t baseY) {
  const int8_t cells[][2] = {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1},
                             {1, 0}, {2, 0}, {3, 0}, {2, -1}};
  for (const auto &cell : cells) {
    setPixel(baseY + cell[1], baseX + cell[0]);
  }
}

void resetDrops() {
  for (uint8_t i = 0; i < MAX_DROPS; i++) {
    state.drops[i].active = false;
  }
}

void resetFlakes() {
  for (uint8_t i = 0; i < MAX_FLAKES; i++) {
    state.flakes[i].active = false;
  }
}

void resetBolts() {
  for (uint8_t i = 0; i < MAX_BOLTS; i++) {
    state.bolts[i].active = false;
  }
  state.flashFrames = 0;
  state.nextStrikeIn = static_cast<int8_t>(random(8, 18));
}

void spawnDrop(Drop *pool, uint8_t count, int8_t minX, int8_t maxX,
               int8_t minSpeed, int8_t maxSpeed) {
  for (uint8_t i = 0; i < count; i++) {
    if (!pool[i].active) {
      pool[i].active = true;
      pool[i].x = random(minX, maxX + 1);
      pool[i].y = random(-3, 0);
      pool[i].speed = random(minSpeed, maxSpeed + 1);
      return;
    }
  }
}

void advanceDrops(Drop *pool, uint8_t count, bool diagonal) {
  for (uint8_t i = 0; i < count; i++) {
    if (!pool[i].active) {
      continue;
    }
    pool[i].y = static_cast<int8_t>(pool[i].y + pool[i].speed);
    if (diagonal && (state.frame % 2) == 0) {
      pool[i].x = static_cast<int8_t>(pool[i].x - 1);
    }
    if (pool[i].y >= DISPLAY_HEIGHT || pool[i].x < 0) {
      pool[i].active = false;
    }
  }
}

void drawDrops(const Drop *pool, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (pool[i].active) {
      setPixel(pool[i].y, pool[i].x);
    }
  }
}

void drawBoltShape(int16_t originX, int8_t variant) {
  // Zigzag trunk + short branch. Coordinates are (dx, dy).
  static const int8_t shapeA[][2] = {
      {0, 0},  {0, 1},  {1, 1},  {1, 2},  {0, 3},  {0, 4},
      {-1, 4}, {-1, 5}, {0, 6},  {0, 7},  {2, 3},  {3, 4}};
  static const int8_t shapeB[][2] = {
      {0, 0},  {-1, 1}, {-1, 2}, {0, 2},  {0, 3},  {1, 4},
      {1, 5},  {0, 6},  {0, 7},  {-2, 3}, {-3, 4}, {2, 5}};
  static const int8_t shapeC[][2] = {
      {0, 0}, {1, 1}, {1, 2}, {0, 3}, {-1, 3}, {-1, 4},
      {0, 5}, {1, 6}, {1, 7}, {2, 2}, {3, 3},  {-2, 5}};

  const int8_t (*cells)[2] = shapeA;
  uint8_t count = sizeof(shapeA) / sizeof(shapeA[0]);
  if (variant % 3 == 1) {
    cells = shapeB;
    count = sizeof(shapeB) / sizeof(shapeB[0]);
  } else if (variant % 3 == 2) {
    cells = shapeC;
    count = sizeof(shapeC) / sizeof(shapeC[0]);
  }

  for (uint8_t i = 0; i < count; i++) {
    int16_t dx = cells[i][0];
    int16_t dy = cells[i][1];
    setPixel(dy, originX + dx);
    // Thicken the bolt by one neighbor for readability on the matrix.
    setPixel(dy, originX + dx + 1);
  }
}

void drawFlashFill() {
  for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
    for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
      if (((row + col) & 1) == 0) {
        setPixel(row, col);
      }
    }
  }
}

void triggerLightning() {
  uint8_t boltCount = random(1, MAX_BOLTS + 1);
  for (uint8_t i = 0; i < MAX_BOLTS; i++) {
    state.bolts[i].active = false;
  }
  for (uint8_t i = 0; i < boltCount; i++) {
    state.bolts[i].active = true;
    state.bolts[i].originX = static_cast<int8_t>(random(3, DISPLAY_WIDTH - 4));
    state.bolts[i].variant = static_cast<int8_t>(random(0, 3));
    state.bolts[i].life = BOLT_HOLD_FRAMES;
  }
  state.flashFrames = FLASH_FRAMES;
  matrix()->control(MD_MAX72XX::INTENSITY, 15);
  state.nextStrikeIn = static_cast<int8_t>(random(10, 28));
}

void tickLightning() {
  if (state.flashFrames > 0) {
    state.flashFrames--;
    if (state.flashFrames == 0) {
      matrix()->control(MD_MAX72XX::INTENSITY, state.brightness);
    }
  }

  bool anyBolt = false;
  for (uint8_t i = 0; i < MAX_BOLTS; i++) {
    if (!state.bolts[i].active) {
      continue;
    }
    state.bolts[i].life--;
    if (state.bolts[i].life <= 0) {
      state.bolts[i].active = false;
    } else {
      anyBolt = true;
    }
  }

  if (!anyBolt && state.flashFrames <= 0) {
    if (state.nextStrikeIn > 0) {
      state.nextStrikeIn--;
    } else {
      triggerLightning();
    }
  }
}

void drawLightning() {
  if (state.flashFrames > 0) {
    drawFlashFill();
  }
  for (uint8_t i = 0; i < MAX_BOLTS; i++) {
    if (state.bolts[i].active) {
      drawBoltShape(state.bolts[i].originX, state.bolts[i].variant);
    }
  }
}

void enterScene(Scene scene, unsigned long now) {
  state.scene = scene;
  state.sceneStartMs = now;
  state.frame = 0;
  resetDrops();
  resetFlakes();
  resetBolts();
  matrix()->control(MD_MAX72XX::INTENSITY, state.brightness);
}

void nextScene(unsigned long now) {
  uint8_t next =
      (static_cast<uint8_t>(state.scene) + 1) % static_cast<uint8_t>(Scene::Count);
  enterScene(static_cast<Scene>(next), now);
}

void renderSunny() {
  drawSun(8, 3, state.frame);
  for (uint8_t x = 18; x < DISPLAY_WIDTH; x++) {
    if ((x + state.frame / 3) % 4 != 0) {
      setPixel(7, x);
    }
  }
}

void renderCloudy() {
  drawSun(6, 2, state.frame);
  int8_t drift = static_cast<int8_t>((state.frame / 3) % 18) - 2;
  drawCloud(10 + drift, 2);
  drawCloud(22 + ((drift + 5) % 10), 3);
}

void renderRainy(bool stormy) {
  int8_t drift = static_cast<int8_t>((state.frame / 4) % 8);
  drawCloud(4 + drift, 1);
  drawCloud(14 + ((drift + 3) % 6), 1);
  drawCloud(24 + ((drift + 1) % 5), 1);

  if ((state.frame % 2) == 0) {
    spawnDrop(state.drops, MAX_DROPS, 0, DISPLAY_WIDTH - 1, 1, stormy ? 2 : 1);
  }
  advanceDrops(state.drops, MAX_DROPS, true);
  drawDrops(state.drops, MAX_DROPS);

  if (stormy) {
    tickLightning();
    drawLightning();
  }
}

void renderSnowy() {
  int8_t drift = static_cast<int8_t>((state.frame / 5) % 6);
  drawCloud(6 + drift, 1);
  drawCloud(18 + ((drift + 2) % 5), 1);

  if ((state.frame % 3) == 0) {
    spawnDrop(state.flakes, MAX_FLAKES, 0, DISPLAY_WIDTH - 1, 1, 1);
  }
  for (uint8_t i = 0; i < MAX_FLAKES; i++) {
    if (!state.flakes[i].active) {
      continue;
    }
    state.flakes[i].y =
        static_cast<int8_t>(state.flakes[i].y + state.flakes[i].speed);
    if ((state.frame + i) % 4 == 0) {
      state.flakes[i].x =
          static_cast<int8_t>(state.flakes[i].x + (((i % 2) == 0) ? 1 : -1));
    }
    if (state.flakes[i].y >= DISPLAY_HEIGHT) {
      state.flakes[i].active = false;
    }
  }
  drawDrops(state.flakes, MAX_FLAKES);

  for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
    if ((x + state.frame / 6) % 3 != 0) {
      setPixel(7, x);
    }
  }
}

void renderFrame() {
  beginFrame();
  switch (state.scene) {
  case Scene::Sunny:
    renderSunny();
    break;
  case Scene::Cloudy:
    renderCloudy();
    break;
  case Scene::Rainy:
    renderRainy(false);
    break;
  case Scene::Stormy:
    renderRainy(true);
    break;
  case Scene::Snowy:
    renderSnowy();
    break;
  case Scene::Count:
  default:
    break;
  }
  endFrame();
}
} // namespace

void weatherWatchStart(const ProgramConfig &cfg) {
  state.brightness = sanitizedBrightness(cfg.brightness);
  Display.setIntensity(state.brightness);
  Display.displayClear();
  enterScene(Scene::Sunny, millis());
  renderFrame();
}

void weatherWatchTick(const ProgramConfig &cfg) {
  state.brightness = sanitizedBrightness(cfg.brightness);
  unsigned long now = millis();
  if (now - state.lastFrameMs < FRAME_MS) {
    return;
  }
  state.lastFrameMs = now;
  state.frame++;

  if (now - state.sceneStartMs >= SCENE_MS) {
    nextScene(now);
  }

  renderFrame();
}
