#include "fireworks.h"

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <esp_system.h>
#include <math.h>

namespace {
constexpr uint8_t MATRIX_HEIGHT = 8;
constexpr uint8_t MATRIX_WIDTH = 32;
constexpr bool DEBUG_FIREWORKS_BRIGHTNESS = false;
constexpr uint8_t MAX_SIMULTANEOUS_FIREWORKS = 8;
constexpr uint8_t MAX_SIMULTANEOUS_GROUND_COMETS = 7;
constexpr unsigned long MIN_FUSE_DELAY_MS = 120UL;
constexpr unsigned long MAX_FUSE_DELAY_MS = 420UL;
constexpr unsigned long MIN_GROUND_COMET_SEQUENCE_DELAY_MS = 80UL;
constexpr unsigned long MAX_GROUND_COMET_SEQUENCE_DELAY_MS = 260UL;
constexpr float FIREWORK_GRAVITY = 0.34f;
constexpr float MIN_TRAJECTORY_ANGLE_DEG = 75.0f;
constexpr float MAX_TRAJECTORY_ANGLE_DEG = 105.0f;
constexpr float CURVE_ACCEL = 0.035f;
constexpr float GROUND_COMET_VERTICAL_SPEED = 1.35f;
constexpr float GROUND_COMET_DECEL = 0.11f;
constexpr float MAX_GROUND_COMET_ANGLE_DEG = 28.0f;
constexpr uint8_t MIN_EXPLOSION_ALTITUDE = 3;
constexpr uint8_t MAX_EXPLOSION_ALTITUDE = 6;
constexpr uint8_t MORTAR_TRAIL_LENGTH = 3;
constexpr uint8_t MIN_GROUND_COMETS = 4;
constexpr uint8_t MAX_GROUND_COMETS = 8;
constexpr uint8_t GROUND_COMET_TRAIL_LENGTH = 8;
constexpr uint8_t GROUND_COMET_MAX_FRAMES = 12;
constexpr uint8_t MIN_CHRYSANTHEMUM_STARS = 18;
constexpr uint8_t MAX_CHRYSANTHEMUM_STARS = 30;
constexpr float BURST_TIME_STEP = 0.42f;
constexpr float MIN_CHRYSANTHEMUM_DEPTH_SCALE = 0.72f;
constexpr float MAX_CHRYSANTHEMUM_DEPTH_SCALE = 1.28f;
constexpr float CHRYSANTHEMUM_DEPTH_PERSPECTIVE = 0.035f;
constexpr float MIN_CHRYSANTHEMUM_STAR_SPEED = 1.15f;
constexpr float MAX_CHRYSANTHEMUM_STAR_SPEED = 1.82f;
constexpr float CHRYSANTHEMUM_WILLOW_SIZE_SCALE = 1.80f;
constexpr float CHRYSANTHEMUM_BURST_DRAG = 0.14f;
constexpr float WILLOW_BURST_DRAG = 0.20f;
constexpr float CHRYSANTHEMUM_BURST_GRAVITY_SCALE = 0.92f;
constexpr float WILLOW_BURST_GRAVITY_SCALE = 0.78f;
constexpr float BURST_GRAVITY_DELAY = 0.70f;
constexpr uint16_t CHRYSANTHEMUM_KIND_WEIGHT = 650;
constexpr uint16_t GROUND_COMET_LAUNCH_WEIGHT = 20;
constexpr uint16_t WILLOW_KIND_WEIGHT = 20;
constexpr uint16_t BROCADE_KIND_WEIGHT = 20;
constexpr uint16_t RING_KIND_WEIGHT = 20;
constexpr uint16_t SMILEY_KIND_WEIGHT = 5;
constexpr uint16_t STAR_KIND_WEIGHT = 5;
constexpr uint16_t FIGURE_EIGHT_KIND_WEIGHT = 5;
constexpr uint16_t HEART_KIND_WEIGHT = 5;
constexpr uint16_t DIAMOND_KIND_WEIGHT = 5;
constexpr uint16_t SPIRAL_KIND_WEIGHT = 5;
constexpr uint16_t FISH_KIND_WEIGHT = 5;
constexpr uint16_t SCREEN_FLASH_KIND_WEIGHT = 1;
constexpr uint16_t FINALE_TRIGGER_CHANCE_DENOMINATOR = 10000;
constexpr uint16_t FINALE_TRIGGER_CHANCE_NUMERATOR = 1;
constexpr uint8_t MIN_FINALE_FIREWORKS = 80;
constexpr uint8_t MAX_FINALE_FIREWORKS = 100;
constexpr uint8_t FINALE_LAUNCH_RATE_MULTIPLIER = 5;
constexpr uint8_t SCREEN_FLASH_ON_FRAMES = 2;
constexpr uint8_t SCREEN_FLASH_OFF_FRAMES = 1;
constexpr uint8_t EMBER_DISSOLVE_MAX_FRAMES = 56;
constexpr uint8_t MIN_EMBER_LIFE_FRAMES = 24;
constexpr float EMBER_MIN_FALL_SPEED = 0.04f;
constexpr float EMBER_MAX_FALL_SPEED = 0.22f;
constexpr float EMBER_MAX_DRIFT_SPEED = 0.10f;
constexpr uint16_t MAX_SCREEN_FLASH_EMBERS = 256;
constexpr uint8_t SCREEN_FLASH_PRE_DISSOLVE_FRAMES =
    SCREEN_FLASH_ON_FRAMES + SCREEN_FLASH_OFF_FRAMES + SCREEN_FLASH_ON_FRAMES +
    SCREEN_FLASH_OFF_FRAMES;
constexpr uint8_t WILLOW_SLOWDOWN_START_FRAME = 5;
constexpr float WILLOW_DRIFT_DAMPING = 0.58f;
constexpr float WILLOW_FALL_SPEED = 0.58f;
constexpr uint8_t WILLOW_TWINKLE_START_FRAME = 4;
constexpr uint8_t MIN_CHRYSANTHEMUM_STAR_LIFE = 10;
constexpr uint8_t MAX_CHRYSANTHEMUM_STAR_LIFE = 14;
constexpr uint8_t MIN_WILLOW_STAR_LIFE = 19;
constexpr uint8_t MAX_WILLOW_STAR_LIFE = 24;
constexpr uint8_t MIN_FISH_STAR_LIFE = 14;
constexpr uint8_t MAX_FISH_STAR_LIFE = 18;
constexpr uint8_t MIN_RING_POINT_LIFE = 7;
constexpr uint8_t MAX_RING_POINT_LIFE = 11;
constexpr uint8_t RING_POINTS = 18;
constexpr uint8_t SHAPED_BURST_TWINKLE_FRAME = 9;
constexpr uint8_t SHAPED_BURST_LIFE = 18;
constexpr int8_t SHAPED_BURST_CENTER_ROW = 3;
constexpr int16_t SHAPED_BURST_HALF_WIDTH = 6;
constexpr float SHAPED_BURST_GRAVITY_SCALE = 0.55f;
constexpr float PI_F = 3.14159265f;

enum class FireworkKind : uint8_t {
  Chrysanthemum,
  Willow,
  Ring,
  Smiley,
  Star,
  FigureEight,
  Heart,
  Brocade,
  Diamond,
  Spiral,
  Fish,
  ScreenFlash
};
enum class FireworkPhase : uint8_t { Mortar, Fuse, Burst };
enum class GroundCometMode : uint8_t { LeftToRight, RightToLeft, AllAtOnce };
enum class ScreenFlashBurstPhase : uint8_t {
  Flash1On,
  Flash1Off,
  Flash2On,
  Flash2Off,
  Dissolve
};

struct MortarPath {
  float x = 0;
  float y = 0;
  float vx = 0;
  float vy = 0;
  float curveAccel = 0;
  float targetY = MIN_EXPLOSION_ALTITUDE;
  int8_t trailRows[MORTAR_TRAIL_LENGTH] = {0};
  int16_t trailCols[MORTAR_TRAIL_LENGTH] = {0};
  uint8_t trailCount = 0;
};

struct StarVelocity {
  float vx = 0;
  float vy = 0;
  float vz = 0;
};

struct StarPosition {
  float x = 0;
  float y = 0;
  float z = 0;
};

struct BurstStar {
  StarVelocity velocity;
  uint8_t twinkleOffset = 0;
  uint8_t lifeEndFrame = 0;
};

struct ScreenFlashEmber {
  int16_t col = 0;
  float originY = 0;
  float vy = 0;
  float vx = 0;
  uint8_t twinkleOffset = 0;
  uint8_t twinklePeriod = 2;
  uint8_t lifeEndFrame = 0;
};

struct FireworkInstance {
  bool active = false;
  FireworkPhase phase = FireworkPhase::Mortar;
  FireworkKind kind = FireworkKind::Ring;
  MortarPath mortar;
  BurstStar chrysanthemumStars[MAX_CHRYSANTHEMUM_STARS];
  uint8_t chrysanthemumStarCount = 0;
  uint8_t frame = 0;
  int8_t centerRow = 3;
  int16_t centerCol = 16;
  float burstRotation = 0;
  float burstScale = 1.0f;
  float ringMajorAxis = 4.0f;
  float ringMinorAxis = 2.0f;
  uint8_t ringPointLifeEnd[RING_POINTS] = {0};
  bool screenFlashEmbersReady = false;
  unsigned long lastFrameMs = 0;
  unsigned long fuseEndMs = 0;
};

struct GroundComet {
  bool scheduled = false;
  bool launched = false;
  unsigned long launchMs = 0;
  unsigned long lastFrameMs = 0;
  float x = 0;
  float y = 0;
  float vx = 0;
  float vy = 0;
  uint8_t frame = 0;
  uint8_t twinkleOffset = 0;
  int8_t trailRows[GROUND_COMET_TRAIL_LENGTH] = {0};
  int16_t trailCols[GROUND_COMET_TRAIL_LENGTH] = {0};
  uint8_t trailCount = 0;
};

struct FireworksSystem {
  bool seeded = false;
  unsigned long nextLaunchMs = 0;
  bool finaleActive = false;
  uint8_t finaleFireworksRemaining = 0;
};

FireworkInstance fireworks[MAX_SIMULTANEOUS_FIREWORKS];
GroundComet groundComets[MAX_SIMULTANEOUS_GROUND_COMETS];
ScreenFlashEmber screenFlashEmbers[MAX_SCREEN_FLASH_EMBERS];
uint16_t screenFlashEmberCount = 0;
FireworksSystem fireworksSystem;
uint32_t litPixelRows[MATRIX_HEIGHT] = {0};
uint16_t litPixelCount = 0;

void resetAllFireworks();
void resetAllGroundComets();

MD_MAX72XX *matrix() { return Display.getGraphicObject(); }

uint16_t matrixWidth() { return matrix()->getColumnCount(); }

bool isShapedKind(FireworkKind kind) {
  return kind == FireworkKind::Smiley || kind == FireworkKind::Star ||
         kind == FireworkKind::FigureEight || kind == FireworkKind::Heart ||
         kind == FireworkKind::Diamond || kind == FireworkKind::Spiral;
}

unsigned long sanitizedFrameMs(const ProgramConfig &cfg) {
  return cfg.fireworksAnimSpeedMs > 0 ? cfg.fireworksAnimSpeedMs : 1UL;
}

unsigned long sanitizedMinLaunchDelay(const ProgramConfig &cfg) {
  return cfg.fireworksMinLaunchDelayMs > 0 ? cfg.fireworksMinLaunchDelayMs
                                           : 1UL;
}

unsigned long sanitizedMaxLaunchDelay(const ProgramConfig &cfg) {
  unsigned long minDelay = sanitizedMinLaunchDelay(cfg);
  unsigned long maxDelay = cfg.fireworksMaxLaunchDelayMs >= minDelay
                               ? cfg.fireworksMaxLaunchDelayMs
                               : minDelay;
  return maxDelay;
}

bool timeReached(unsigned long now, unsigned long target) {
  return (int32_t)(now - target) >= 0;
}

uint8_t sanitizedBrightness(uint8_t brightness) {
  return brightness > 15 ? 15 : brightness;
}

uint8_t sanitizedFireworksMaxBrightness(const ProgramConfig &cfg) {
  uint8_t minBrightness = sanitizedBrightness(cfg.brightness);
  uint8_t maxBrightness = sanitizedBrightness(cfg.fireworksMaxBrightness);
  return max(maxBrightness, minBrightness);
}

float randomFloat(float minValue, float maxValue) {
  return minValue + (maxValue - minValue) * (random(0, 10001) / 10000.0f);
}

int16_t clampCol(int16_t col) {
  int16_t maxCol = matrixWidth() - 1;
  if (col < 0) {
    return 0;
  }
  if (col > maxCol) {
    return maxCol;
  }
  return col;
}

int16_t clampShapedCenterCol(int16_t col) {
  int16_t maxCol = matrixWidth() - 1;
  if (maxCol < SHAPED_BURST_HALF_WIDTH * 2) {
    return clampCol(col);
  }

  int16_t minCol = SHAPED_BURST_HALF_WIDTH;
  int16_t safeMaxCol = maxCol - SHAPED_BURST_HALF_WIDTH;
  if (col < minCol) {
    return minCol;
  }
  if (col > safeMaxCol) {
    return safeMaxCol;
  }
  return col;
}

int8_t rowFromAltitude(float altitude) {
  return MATRIX_HEIGHT - 1 - (int8_t)roundf(altitude);
}

StarPosition starPositionFromVelocity(const StarVelocity &velocity,
                                      uint8_t frame, float gravityScale,
                                      float burstDrag) {
  float age = frame * BURST_TIME_STEP;
  float burstTravel = (1.0f - expf(-age * burstDrag)) / burstDrag;
  float gravityAge = max(0.0f, age - BURST_GRAVITY_DELAY);
  StarPosition position;
  position.x = velocity.vx * burstTravel;
  position.y = velocity.vy * burstTravel +
               0.5f * FIREWORK_GRAVITY * gravityScale * gravityAge * gravityAge;
  position.z = velocity.vz * burstTravel;
  return position;
}

void setPixel(int8_t row, int16_t col) {
  if (row < 0 || row >= MATRIX_HEIGHT || col < 0 || col >= matrixWidth()) {
    return;
  }
  if (col < MATRIX_WIDTH) {
    uint32_t pixelMask = 1UL << col;
    if ((litPixelRows[row] & pixelMask) == 0) {
      litPixelRows[row] |= pixelMask;
      litPixelCount++;
    }
  }
  matrix()->setPoint(row, col, true);
}

void beginFrame() {
  for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
    litPixelRows[row] = 0;
  }
  litPixelCount = 0;
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  matrix()->clear();
}

uint8_t calculateFireworksBrightness(const ProgramConfig &cfg,
                                     float &brightnessPercent) {
  uint8_t minBrightness = sanitizedBrightness(cfg.brightness);
  uint8_t maxBrightness = sanitizedFireworksMaxBrightness(cfg);
  uint16_t width = matrixWidth();
  uint16_t trackedWidth = width < MATRIX_WIDTH ? width : MATRIX_WIDTH;
  uint16_t pixelCapacity = MATRIX_HEIGHT * trackedWidth;
  if (pixelCapacity == 0) {
    brightnessPercent = 0.0f;
    return minBrightness;
  }

  brightnessPercent = (litPixelCount * 100.0f) / pixelCapacity;
  if (maxBrightness == minBrightness) {
    return minBrightness;
  }

  float brightnessRange = maxBrightness - minBrightness;
  float scaledBrightness =
      minBrightness + (brightnessPercent / 100.0f) * brightnessRange;
  uint8_t brightness = (uint8_t)roundf(scaledBrightness);
  return brightness > maxBrightness ? maxBrightness : brightness;
}

void applyFireworksBrightness(const ProgramConfig &cfg) {
  float brightnessPercent = 0.0f;
  uint8_t brightness = calculateFireworksBrightness(cfg, brightnessPercent);
  matrix()->control(MD_MAX72XX::INTENSITY, brightness);

  if (DEBUG_FIREWORKS_BRIGHTNESS) {
    Serial.print("Fireworks brightness: ");
    Serial.print(brightnessPercent, 1);
    Serial.print("% -> ");
    Serial.println(brightness);
  }
}

void endFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix()->update();
}

void fillScreen() {
  uint16_t width = matrixWidth();
  for (int8_t row = 0; row < MATRIX_HEIGHT; row++) {
    for (int16_t col = 0; col < width; col++) {
      setPixel(row, col);
    }
  }
}

void initScreenFlashEmbers(FireworkInstance &fw) {
  if (fw.screenFlashEmbersReady) {
    return;
  }

  uint16_t width = matrixWidth();
  screenFlashEmberCount = 0;
  for (int8_t row = 0; row < MATRIX_HEIGHT; row++) {
    for (int16_t col = 0; col < width; col++) {
      if (screenFlashEmberCount >= MAX_SCREEN_FLASH_EMBERS) {
        break;
      }

      ScreenFlashEmber &ember = screenFlashEmbers[screenFlashEmberCount++];
      ember.col = col;
      ember.originY = row;
      ember.vy = randomFloat(EMBER_MIN_FALL_SPEED, EMBER_MAX_FALL_SPEED);
      ember.vx = randomFloat(-EMBER_MAX_DRIFT_SPEED, EMBER_MAX_DRIFT_SPEED);
      ember.twinkleOffset = random(0, 7);
      ember.twinklePeriod = random(2, 5);
      ember.lifeEndFrame =
          random(MIN_EMBER_LIFE_FRAMES, EMBER_DISSOLVE_MAX_FRAMES + 1);
    }
  }

  fw.screenFlashEmbersReady = true;
}

bool screenFlashEmberVisible(const ScreenFlashEmber &ember,
                             uint8_t dissolveFrame) {
  if (dissolveFrame > ember.lifeEndFrame) {
    return false;
  }

  float y = ember.originY + dissolveFrame * ember.vy;
  if (y >= MATRIX_HEIGHT) {
    return false;
  }

  if (((dissolveFrame + ember.twinkleOffset) % ember.twinklePeriod) == 0) {
    return false;
  }

  uint8_t remainingLife = ember.lifeEndFrame - dissolveFrame;
  if (remainingLife < 10 && ((dissolveFrame + ember.twinkleOffset) % 3) == 0) {
    return false;
  }
  if (remainingLife < 5 && ((dissolveFrame + ember.twinkleOffset) % 2) == 0) {
    return false;
  }

  return true;
}

bool screenFlashDissolveHasLiveEmbers(uint8_t dissolveFrame) {
  for (uint16_t i = 0; i < screenFlashEmberCount; i++) {
    if (screenFlashEmberVisible(screenFlashEmbers[i], dissolveFrame)) {
      return true;
    }
  }
  return false;
}

void drawScreenFlashEmbers(uint8_t dissolveFrame) {
  for (uint16_t i = 0; i < screenFlashEmberCount; i++) {
    const ScreenFlashEmber &ember = screenFlashEmbers[i];
    if (!screenFlashEmberVisible(ember, dissolveFrame)) {
      continue;
    }

    float y = ember.originY + dissolveFrame * ember.vy;
    int16_t col = ember.col + (int16_t)roundf(dissolveFrame * ember.vx);
    setPixel((int8_t)roundf(y), col);
  }
}

ScreenFlashBurstPhase screenFlashBurstPhase(uint8_t frame) {
  if (frame < SCREEN_FLASH_ON_FRAMES) {
    return ScreenFlashBurstPhase::Flash1On;
  }
  frame -= SCREEN_FLASH_ON_FRAMES;
  if (frame < SCREEN_FLASH_OFF_FRAMES) {
    return ScreenFlashBurstPhase::Flash1Off;
  }
  frame -= SCREEN_FLASH_OFF_FRAMES;
  if (frame < SCREEN_FLASH_ON_FRAMES) {
    return ScreenFlashBurstPhase::Flash2On;
  }
  frame -= SCREEN_FLASH_ON_FRAMES;
  if (frame < SCREEN_FLASH_OFF_FRAMES) {
    return ScreenFlashBurstPhase::Flash2Off;
  }
  return ScreenFlashBurstPhase::Dissolve;
}

uint8_t screenFlashDissolveFrame(uint8_t frame) {
  return frame - SCREEN_FLASH_PRE_DISSOLVE_FRAMES;
}

void drawScreenFlashBurst(const FireworkInstance &fw) {
  switch (screenFlashBurstPhase(fw.frame)) {
  case ScreenFlashBurstPhase::Flash1On:
  case ScreenFlashBurstPhase::Flash2On:
    fillScreen();
    break;
  case ScreenFlashBurstPhase::Dissolve:
    drawScreenFlashEmbers(screenFlashDissolveFrame(fw.frame));
    break;
  default:
    break;
  }
}

void setRotatedPoint(int8_t centerRow, int16_t centerCol, float x, float y,
                     float rotation) {
  float cosA = cosf(rotation);
  float sinA = sinf(rotation);
  int16_t col = centerCol + (int16_t)roundf(x * cosA - y * sinA);
  int8_t row = centerRow + (int8_t)roundf(x * sinA + y * cosA);
  setPixel(row, col);
}

int8_t shapedBurstCenterRow(const FireworkInstance &fw) {
  float age = fw.frame * BURST_TIME_STEP;
  float fall = 0.5f * FIREWORK_GRAVITY * SHAPED_BURST_GRAVITY_SCALE * age * age;
  return fw.centerRow + (int8_t)roundf(fall);
}

bool shapedBurstPointVisible(const FireworkInstance &fw, uint8_t pointIndex) {
  if (fw.frame < SHAPED_BURST_TWINKLE_FRAME) {
    return true;
  }

  uint8_t fadeAge = fw.frame - SHAPED_BURST_TWINKLE_FRAME;
  uint8_t twinklePeriod = max(2, 4 - min((int)fadeAge, 2));
  if ((pointIndex + fw.frame) % twinklePeriod == 0) {
    return false;
  }

  if (fadeAge >= 7) {
    return (pointIndex * 5 + fw.frame) % 5 == 0;
  }

  return fadeAge < 4 || (pointIndex * 3 + fw.frame) % 3 != 0;
}

void drawRotatedLine(int8_t centerRow, int16_t centerCol, float startX,
                     float startY, float endX, float endY, float rotation) {
  float dx = endX - startX;
  float dy = endY - startY;
  uint8_t steps = max(1, (int)ceilf(max(fabsf(dx), fabsf(dy)) * 2.0f));
  for (uint8_t i = 0; i <= steps; i++) {
    float amount = i / (float)steps;
    setRotatedPoint(centerRow, centerCol, startX + dx * amount,
                    startY + dy * amount, rotation);
  }
}

void drawTwinkledRotatedLine(const FireworkInstance &fw, uint8_t lineIndex,
                             float startX, float startY, float endX, float endY,
                             float rotation) {
  float dx = endX - startX;
  float dy = endY - startY;
  uint8_t steps = max(1, (int)ceilf(max(fabsf(dx), fabsf(dy)) * 2.0f));
  int8_t centerRow = shapedBurstCenterRow(fw);

  for (uint8_t i = 0; i <= steps; i++) {
    if (!shapedBurstPointVisible(fw, lineIndex * 9 + i)) {
      continue;
    }

    float amount = i / (float)steps;
    setRotatedPoint(centerRow, fw.centerCol, startX + dx * amount,
                    startY + dy * amount, rotation);
  }
}

void addMortarTrailPoint(MortarPath &path) {
  int8_t row = rowFromAltitude(path.y);
  int16_t col = clampCol((int16_t)roundf(path.x));

  if (path.trailCount < MORTAR_TRAIL_LENGTH) {
    path.trailRows[path.trailCount] = row;
    path.trailCols[path.trailCount] = col;
    path.trailCount++;
    return;
  }

  for (uint8_t i = 1; i < MORTAR_TRAIL_LENGTH; i++) {
    path.trailRows[i - 1] = path.trailRows[i];
    path.trailCols[i - 1] = path.trailCols[i];
  }
  path.trailRows[MORTAR_TRAIL_LENGTH - 1] = row;
  path.trailCols[MORTAR_TRAIL_LENGTH - 1] = col;
}

void addGroundCometTrailPoint(GroundComet &comet) {
  int8_t row = rowFromAltitude(comet.y);
  int16_t col = clampCol((int16_t)roundf(comet.x));

  if (comet.trailCount < GROUND_COMET_TRAIL_LENGTH) {
    comet.trailRows[comet.trailCount] = row;
    comet.trailCols[comet.trailCount] = col;
    comet.trailCount++;
    return;
  }

  for (uint8_t i = 1; i < GROUND_COMET_TRAIL_LENGTH; i++) {
    comet.trailRows[i - 1] = comet.trailRows[i];
    comet.trailCols[i - 1] = comet.trailCols[i];
  }
  comet.trailRows[GROUND_COMET_TRAIL_LENGTH - 1] = row;
  comet.trailCols[GROUND_COMET_TRAIL_LENGTH - 1] = col;
}

MortarPath createMortarPath() {
  MortarPath path;
  uint16_t width = matrixWidth();
  path.x = random(2, max(3, (int)width - 2));
  path.y = 0;
  path.targetY = random(MIN_EXPLOSION_ALTITUDE, MAX_EXPLOSION_ALTITUDE + 1);

  float angleDeg =
      randomFloat(MIN_TRAJECTORY_ANGLE_DEG, MAX_TRAJECTORY_ANGLE_DEG);
  float angleRad = angleDeg * PI_F / 180.0f;
  float verticalVelocity = sqrtf(2.0f * FIREWORK_GRAVITY * path.targetY);
  float launchVelocity = verticalVelocity / max(0.15f, sinf(angleRad));
  path.vx = cosf(angleRad) * launchVelocity;
  path.vy = sinf(angleRad) * launchVelocity;

  if (random(0, 100) < 55) {
    path.curveAccel = random(0, 2) == 0 ? -CURVE_ACCEL : CURVE_ACCEL;
  }

  addMortarTrailPoint(path);
  return path;
}

bool advanceMortarPath(MortarPath &path) {
  path.x += path.vx;
  path.y += path.vy;
  path.vx += path.curveAccel;
  path.vy -= FIREWORK_GRAVITY;

  if (path.y >= path.targetY || path.vy <= 0) {
    path.y = path.targetY;
    addMortarTrailPoint(path);
    return true;
  }

  addMortarTrailPoint(path);
  return false;
}

void configureChrysanthemumStars(FireworkInstance &fw) {
  fw.chrysanthemumStarCount =
      random(MIN_CHRYSANTHEMUM_STARS, MAX_CHRYSANTHEMUM_STARS + 1);

  uint8_t minLife = MIN_CHRYSANTHEMUM_STAR_LIFE;
  uint8_t maxLife = MAX_CHRYSANTHEMUM_STAR_LIFE;
  if (fw.kind == FireworkKind::Willow || fw.kind == FireworkKind::Brocade) {
    minLife = MIN_WILLOW_STAR_LIFE;
    maxLife = MAX_WILLOW_STAR_LIFE;
  } else if (fw.kind == FireworkKind::Fish) {
    minLife = MIN_FISH_STAR_LIFE;
    maxLife = MAX_FISH_STAR_LIFE;
  }

  for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
    StarVelocity velocity;
    if (fw.kind == FireworkKind::Brocade) {
      float angle = randomFloat(-0.90f * PI_F, -0.10f * PI_F);
      float speed = randomFloat(1.05f, 1.65f) * fw.burstScale * 1.35f;
      velocity.vx = cosf(angle) * speed;
      velocity.vy = sinf(angle) * speed;
      velocity.vz = randomFloat(-0.45f, 0.45f) * speed;
    } else if (fw.kind == FireworkKind::Fish) {
      float side = random(0, 2) == 0 ? -1.0f : 1.0f;
      float speed = randomFloat(0.90f, 1.55f) * fw.burstScale;
      velocity.vx = side * speed;
      velocity.vy = randomFloat(-0.70f, 0.45f) * speed;
      velocity.vz = randomFloat(-0.35f, 0.35f) * speed;
    } else {
      float azimuth = randomFloat(0.0f, 2.0f * PI_F);
      float depth = randomFloat(-1.0f, 1.0f);
      float screenRadius = sqrtf(max(0.0f, 1.0f - depth * depth));
      float speed = randomFloat(MIN_CHRYSANTHEMUM_STAR_SPEED,
                                MAX_CHRYSANTHEMUM_STAR_SPEED) *
                    fw.burstScale * CHRYSANTHEMUM_WILLOW_SIZE_SCALE;
      velocity.vx = cosf(azimuth) * screenRadius * speed;
      velocity.vy = sinf(azimuth) * screenRadius * speed;
      velocity.vz = depth * speed;
    }

    fw.chrysanthemumStars[i].velocity = velocity;
    fw.chrysanthemumStars[i].twinkleOffset = random(0, 3);
    fw.chrysanthemumStars[i].lifeEndFrame = random(minLife, maxLife + 1);
  }
}

void configureRingPointLifetimes(FireworkInstance &fw) {
  for (uint8_t i = 0; i < RING_POINTS; i++) {
    fw.ringPointLifeEnd[i] =
        random(MIN_RING_POINT_LIFE, MAX_RING_POINT_LIFE + 1);
  }
}

bool starAlive(uint8_t frame, uint8_t lifeEndFrame) {
  return frame <= lifeEndFrame;
}

bool burstHasLiveContent(const FireworkInstance &fw) {
  switch (fw.kind) {
  case FireworkKind::Chrysanthemum:
  case FireworkKind::Willow:
  case FireworkKind::Brocade:
  case FireworkKind::Fish:
    for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
      if (starAlive(fw.frame, fw.chrysanthemumStars[i].lifeEndFrame)) {
        return true;
      }
    }
    return false;
  case FireworkKind::Ring:
    for (uint8_t i = 0; i < RING_POINTS; i++) {
      if (starAlive(fw.frame, fw.ringPointLifeEnd[i])) {
        return true;
      }
    }
    return false;
  case FireworkKind::Smiley:
  case FireworkKind::Star:
  case FireworkKind::FigureEight:
  case FireworkKind::Heart:
  case FireworkKind::Diamond:
  case FireworkKind::Spiral:
    return fw.frame <= SHAPED_BURST_LIFE;
  case FireworkKind::ScreenFlash:
    if (screenFlashBurstPhase(fw.frame) != ScreenFlashBurstPhase::Dissolve) {
      return true;
    }
    return screenFlashDissolveHasLiveEmbers(screenFlashDissolveFrame(fw.frame));
  }
  return false;
}

float chrysanthemumDepthScale(float z) {
  return constrain(1.0f - z * CHRYSANTHEMUM_DEPTH_PERSPECTIVE,
                   MIN_CHRYSANTHEMUM_DEPTH_SCALE,
                   MAX_CHRYSANTHEMUM_DEPTH_SCALE);
}

void drawChrysanthemumBurst(const FireworkInstance &fw) {
  for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
    const BurstStar &star = fw.chrysanthemumStars[i];
    if (!starAlive(fw.frame, star.lifeEndFrame)) {
      continue;
    }
    if (fw.frame > 6 && (fw.frame + star.twinkleOffset) % 3 == 0) {
      continue;
    }

    StarPosition position = starPositionFromVelocity(
        star.velocity, fw.frame, CHRYSANTHEMUM_BURST_GRAVITY_SCALE,
        CHRYSANTHEMUM_BURST_DRAG);
    float depthScale = chrysanthemumDepthScale(position.z);
    setPixel(fw.centerRow + (int8_t)roundf(position.y * depthScale),
             fw.centerCol + (int16_t)roundf(position.x * depthScale));
  }
}

StarPosition willowStarPosition(const StarVelocity &velocity, uint8_t frame) {
  if (frame <= WILLOW_SLOWDOWN_START_FRAME) {
    return starPositionFromVelocity(velocity, frame, WILLOW_BURST_GRAVITY_SCALE,
                                    WILLOW_BURST_DRAG);
  }

  StarPosition anchor =
      starPositionFromVelocity(velocity, WILLOW_SLOWDOWN_START_FRAME,
                               WILLOW_BURST_GRAVITY_SCALE, WILLOW_BURST_DRAG);
  float ashAge = (frame - WILLOW_SLOWDOWN_START_FRAME) * BURST_TIME_STEP;
  float driftScale =
      (1.0f - expf(-ashAge * WILLOW_DRIFT_DAMPING)) / WILLOW_DRIFT_DAMPING;

  anchor.x += velocity.vx * 0.28f * driftScale;
  anchor.y += velocity.vy * 0.08f * driftScale + ashAge * WILLOW_FALL_SPEED;
  anchor.z += velocity.vz * 0.22f * driftScale;
  return anchor;
}

void drawWillowBurst(const FireworkInstance &fw) {
  if (fw.frame <= WILLOW_TWINKLE_START_FRAME) {
    setPixel(fw.centerRow, fw.centerCol);
  }

  for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
    const BurstStar &star = fw.chrysanthemumStars[i];
    if (!starAlive(fw.frame, star.lifeEndFrame)) {
      continue;
    }
    if (fw.frame > WILLOW_TWINKLE_START_FRAME &&
        (fw.frame + star.twinkleOffset + i) % 4 == 0) {
      continue;
    }

    StarPosition position = willowStarPosition(star.velocity, fw.frame);
    float depthScale = chrysanthemumDepthScale(position.z);
    setPixel(fw.centerRow + (int8_t)roundf(position.y * depthScale),
             fw.centerCol + (int16_t)roundf(position.x * depthScale));

    if (fw.frame > WILLOW_SLOWDOWN_START_FRAME &&
        (fw.frame + star.twinkleOffset) % 3 != 0) {
      setPixel(fw.centerRow + (int8_t)roundf(position.y * depthScale) - 1,
               fw.centerCol + (int16_t)roundf(position.x * depthScale));
    }
  }
}

void drawBrocadeBurst(const FireworkInstance &fw) {
  if (fw.frame <= WILLOW_TWINKLE_START_FRAME) {
    setPixel(fw.centerRow, fw.centerCol);
  }

  for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
    const BurstStar &star = fw.chrysanthemumStars[i];
    if (!starAlive(fw.frame, star.lifeEndFrame)) {
      continue;
    }
    if (fw.frame > 8 && (fw.frame + star.twinkleOffset + i) % 5 == 0) {
      continue;
    }

    StarPosition position = willowStarPosition(star.velocity, fw.frame);

    float depthScale = chrysanthemumDepthScale(position.z);
    int8_t row = fw.centerRow + (int8_t)roundf(position.y * depthScale);
    int16_t col = fw.centerCol + (int16_t)roundf(position.x * depthScale);
    setPixel(row, col);
    if (fw.frame > WILLOW_SLOWDOWN_START_FRAME &&
        (fw.frame + star.twinkleOffset) % 3 != 1) {
      setPixel(row - 1, col);
      if ((i + fw.frame) % 4 == 0) {
        setPixel(row - 2, col);
      }
    }
  }
}

void drawFishBurst(const FireworkInstance &fw) {
  for (uint8_t i = 0; i < fw.chrysanthemumStarCount; i++) {
    const BurstStar &star = fw.chrysanthemumStars[i];
    if (!starAlive(fw.frame, star.lifeEndFrame)) {
      continue;
    }
    if (fw.frame > 7 && (fw.frame + star.twinkleOffset + i) % 4 == 0) {
      continue;
    }

    StarPosition position = starPositionFromVelocity(
        star.velocity, fw.frame, 0.45f, CHRYSANTHEMUM_BURST_DRAG);
    float swimPhase = fw.frame * 0.85f + i * 0.67f + star.twinkleOffset;
    float swimAmount =
        sinf(swimPhase) * (0.55f + min((int)fw.frame, 8) * 0.04f);
    position.x += swimAmount;
    position.y += cosf(swimPhase * 0.72f) * 0.22f;

    float depthScale = chrysanthemumDepthScale(position.z);
    int8_t row = fw.centerRow + (int8_t)roundf(position.y * depthScale);
    int16_t col = fw.centerCol + (int16_t)roundf(position.x * depthScale);
    int8_t tailSide = star.velocity.vx < 0.0f ? 1 : -1;
    setPixel(row, col);
    if (fw.frame > 2 && (i + fw.frame) % 3 != 0) {
      setPixel(row, col + tailSide);
    }
  }
}

void drawRingBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.32f + fw.frame * 0.17f);
  uint8_t fadeSkip = fw.frame > 6 ? fw.frame - 6 : 0;

  for (uint8_t i = 0; i < RING_POINTS; i++) {
    if (!starAlive(fw.frame, fw.ringPointLifeEnd[i])) {
      continue;
    }
    if (fadeSkip > 0 && i % (fadeSkip + 2) == 0) {
      continue;
    }

    float angle = i * (2.0f * PI_F / RING_POINTS);
    float x = cosf(angle) * fw.ringMajorAxis * scale;
    float y = sinf(angle) * fw.ringMinorAxis * scale;
    setRotatedPoint(fw.centerRow, fw.centerCol, x, y, fw.burstRotation);
  }
}

void drawSmileyBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.18f + fw.frame * 0.18f);
  int8_t centerRow = shapedBurstCenterRow(fw);
  uint8_t facePoints = 30;

  for (uint8_t i = 0; i < facePoints; i++) {
    if (!shapedBurstPointVisible(fw, i)) {
      continue;
    }

    float angle = i * (2.0f * PI_F / facePoints);
    float x = cosf(angle) * 5.4f * scale;
    float y = sinf(angle) * 3.2f * scale;
    setRotatedPoint(centerRow, fw.centerCol, x, y, 0.0f);
  }

  if (shapedBurstPointVisible(fw, 31)) {
    setRotatedPoint(centerRow, fw.centerCol, -2.0f * scale, -1.0f * scale,
                    0.0f);
  }
  if (shapedBurstPointVisible(fw, 32)) {
    setRotatedPoint(centerRow, fw.centerCol, 2.0f * scale, -1.0f * scale, 0.0f);
  }
  if (scale > 0.72f) {
    if (shapedBurstPointVisible(fw, 33)) {
      setRotatedPoint(centerRow, fw.centerCol, -2.0f * scale, -1.8f * scale,
                      0.0f);
    }
    if (shapedBurstPointVisible(fw, 34)) {
      setRotatedPoint(centerRow, fw.centerCol, 2.0f * scale, -1.8f * scale,
                      0.0f);
    }
  }

  for (uint8_t i = 0; i <= 12; i++) {
    if (!shapedBurstPointVisible(fw, 40 + i)) {
      continue;
    }

    float angle = i * (PI_F / 12.0f);
    float x = cosf(angle) * 2.9f * scale;
    float y = (1.0f + sinf(angle) * 1.4f) * scale;
    setRotatedPoint(centerRow, fw.centerCol, x, y, 0.0f);
  }
}

void drawStarBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.16f + fw.frame * 0.18f);
  float vertexX[5];
  float vertexY[5];

  for (uint8_t i = 0; i < 5; i++) {
    float angle = -PI_F / 2.0f + i * (2.0f * PI_F / 5.0f);
    vertexX[i] = cosf(angle) * 5.7f * scale;
    vertexY[i] = sinf(angle) * 3.3f * scale;
  }

  const uint8_t starOrder[] = {0, 2, 4, 1, 3, 0};
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t start = starOrder[i];
    uint8_t end = starOrder[i + 1];
    drawTwinkledRotatedLine(fw, i, vertexX[start], vertexY[start], vertexX[end],
                            vertexY[end], fw.burstRotation);
  }

  if (fw.frame > 4 && shapedBurstPointVisible(fw, 48)) {
    setRotatedPoint(shapedBurstCenterRow(fw), fw.centerCol, 0.0f, 0.0f, 0.0f);
  }
}

void drawFigureEightBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.16f + fw.frame * 0.18f);
  int8_t centerRow = shapedBurstCenterRow(fw);
  uint8_t points = 34;

  for (uint8_t i = 0; i < points; i++) {
    if (!shapedBurstPointVisible(fw, i)) {
      continue;
    }

    float angle = i * (2.0f * PI_F / points);
    float x = sinf(angle) * fw.ringMajorAxis * scale;
    float y = sinf(angle * 2.0f) * fw.ringMinorAxis * scale;
    setRotatedPoint(centerRow, fw.centerCol, x, y, fw.burstRotation);
  }

  if (fw.frame > 3 && shapedBurstPointVisible(fw, 36)) {
    setRotatedPoint(centerRow, fw.centerCol, 0.0f, 0.0f, fw.burstRotation);
  }
}

void drawHeartBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.16f + fw.frame * 0.18f);
  int8_t centerRow = shapedBurstCenterRow(fw);
  uint8_t points = 42;

  for (uint8_t i = 0; i < points; i++) {
    if (!shapedBurstPointVisible(fw, i)) {
      continue;
    }

    float t = i * (2.0f * PI_F / points);
    float sinT = sinf(t);
    float x = sinT * sinT * sinT * fw.ringMajorAxis * scale;
    float heartY = 13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                   2.0f * cosf(3.0f * t) - cosf(4.0f * t);
    float y = -heartY * (fw.ringMinorAxis / 17.0f) * scale;
    setRotatedPoint(centerRow, fw.centerCol, x, y, fw.burstRotation);
  }

  if (fw.frame > 4 && shapedBurstPointVisible(fw, 44)) {
    setRotatedPoint(centerRow, fw.centerCol, 0.0f, 0.7f * scale,
                    fw.burstRotation);
  }
}

void drawDiamondBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.18f + fw.frame * 0.18f);
  float width = 5.5f * scale;
  float height = 3.4f * scale;
  drawTwinkledRotatedLine(fw, 0, 0.0f, -height, width, 0.0f, fw.burstRotation);
  drawTwinkledRotatedLine(fw, 1, width, 0.0f, 0.0f, height, fw.burstRotation);
  drawTwinkledRotatedLine(fw, 2, 0.0f, height, -width, 0.0f, fw.burstRotation);
  drawTwinkledRotatedLine(fw, 3, -width, 0.0f, 0.0f, -height, fw.burstRotation);
}

void drawSpiralBurst(const FireworkInstance &fw) {
  float scale = min(1.0f, 0.14f + fw.frame * 0.17f);
  int8_t centerRow = shapedBurstCenterRow(fw);
  const uint8_t points = 36;
  float twist = fw.frame * 0.16f;

  for (uint8_t i = 0; i < points; i++) {
    if (!shapedBurstPointVisible(fw, i)) {
      continue;
    }

    float amount = i / (float)(points - 1);
    float angle = amount * 3.35f * PI_F + twist;
    float radiusX = amount * fw.ringMajorAxis * scale;
    float radiusY = amount * fw.ringMinorAxis * scale;
    setRotatedPoint(centerRow, fw.centerCol, cosf(angle) * radiusX,
                    sinf(angle) * radiusY, fw.burstRotation);
  }
}

void drawMortarInstance(const FireworkInstance &fw) {
  if (fw.mortar.trailCount == 0) {
    return;
  }

  if (fw.kind == FireworkKind::ScreenFlash) {
    for (uint8_t i = 0; i < fw.mortar.trailCount - 1; i++) {
      setPixel(fw.mortar.trailRows[i], fw.mortar.trailCols[i]);
    }

    int8_t headRow = fw.mortar.trailRows[fw.mortar.trailCount - 1];
    int16_t headCol = fw.mortar.trailCols[fw.mortar.trailCount - 1];
    setPixel(headRow, headCol);
    setPixel(headRow + 1, headCol);
    setPixel(headRow, headCol + 1);
    setPixel(headRow + 1, headCol + 1);
    return;
  }

  for (uint8_t i = 0; i < fw.mortar.trailCount; i++) {
    setPixel(fw.mortar.trailRows[i], fw.mortar.trailCols[i]);
  }
}

void drawGroundComet(const GroundComet &comet) {
  if (!comet.scheduled || !comet.launched || comet.trailCount == 0) {
    return;
  }

  for (uint8_t i = 0; i < comet.trailCount; i++) {
    uint8_t age = comet.trailCount - 1 - i;
    if (age > 2 && (comet.frame + comet.twinkleOffset + age) % 3 == 0) {
      continue;
    }
    setPixel(comet.trailRows[i], comet.trailCols[i]);
  }

  int8_t headRow = comet.trailRows[comet.trailCount - 1];
  int16_t headCol = comet.trailCols[comet.trailCount - 1];
  int8_t side = comet.vx < -0.05f ? -1 : comet.vx > 0.05f ? 1 : 0;
  setPixel(headRow, headCol);
  setPixel(headRow + 1, headCol);
  if (side != 0) {
    setPixel(headRow, headCol + side);
  }
  if (comet.frame < 3) {
    setPixel(MATRIX_HEIGHT - 1, headCol - 1);
    setPixel(MATRIX_HEIGHT - 1, headCol + 1);
  }
}

void drawFireworkInstance(const FireworkInstance &fw) {
  switch (fw.phase) {
  case FireworkPhase::Mortar:
    drawMortarInstance(fw);
    break;
  case FireworkPhase::Fuse:
    break;
  case FireworkPhase::Burst:
    switch (fw.kind) {
    case FireworkKind::Chrysanthemum:
      drawChrysanthemumBurst(fw);
      break;
    case FireworkKind::Willow:
      drawWillowBurst(fw);
      break;
    case FireworkKind::Ring:
      drawRingBurst(fw);
      break;
    case FireworkKind::Smiley:
      drawSmileyBurst(fw);
      break;
    case FireworkKind::Star:
      drawStarBurst(fw);
      break;
    case FireworkKind::FigureEight:
      drawFigureEightBurst(fw);
      break;
    case FireworkKind::Heart:
      drawHeartBurst(fw);
      break;
    case FireworkKind::Brocade:
      drawBrocadeBurst(fw);
      break;
    case FireworkKind::Diamond:
      drawDiamondBurst(fw);
      break;
    case FireworkKind::Spiral:
      drawSpiralBurst(fw);
      break;
    case FireworkKind::Fish:
      drawFishBurst(fw);
      break;
    case FireworkKind::ScreenFlash:
      drawScreenFlashBurst(fw);
      break;
    }
    break;
  }
}

void renderAllFireworks(const ProgramConfig &cfg) {
  beginFrame();
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    if (fireworks[i].active && fireworks[i].kind != FireworkKind::ScreenFlash) {
      drawFireworkInstance(fireworks[i]);
    }
  }
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_GROUND_COMETS; i++) {
    drawGroundComet(groundComets[i]);
  }
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    if (fireworks[i].active && fireworks[i].kind == FireworkKind::ScreenFlash) {
      drawFireworkInstance(fireworks[i]);
    }
  }
  applyFireworksBrightness(cfg);
  endFrame();
}

bool anyFireworkActive() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    if (fireworks[i].active) {
      return true;
    }
  }
  return false;
}

bool anyGroundCometVisible() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_GROUND_COMETS; i++) {
    if (groundComets[i].scheduled && groundComets[i].launched) {
      return true;
    }
  }
  return false;
}

bool anyGroundCometScheduled() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_GROUND_COMETS; i++) {
    if (groundComets[i].scheduled) {
      return true;
    }
  }
  return false;
}

FireworkInstance *findFreeSlot() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    if (!fireworks[i].active) {
      return &fireworks[i];
    }
  }
  return nullptr;
}

void scheduleNextLaunch(unsigned long now, const ProgramConfig &cfg) {
  unsigned long minDelay = sanitizedMinLaunchDelay(cfg);
  unsigned long maxDelay = sanitizedMaxLaunchDelay(cfg);
  if (fireworksSystem.finaleActive) {
    minDelay = max(1UL, minDelay / FINALE_LAUNCH_RATE_MULTIPLIER);
    maxDelay = max(minDelay, maxDelay / FINALE_LAUNCH_RATE_MULTIPLIER);
  }
  fireworksSystem.nextLaunchMs = now + random(minDelay, maxDelay + 1);
}

void registerFireworkLaunch() {
  if (!fireworksSystem.finaleActive &&
      random(0, FINALE_TRIGGER_CHANCE_DENOMINATOR) <
          FINALE_TRIGGER_CHANCE_NUMERATOR) {
    fireworksSystem.finaleActive = true;
    fireworksSystem.finaleFireworksRemaining =
        random(MIN_FINALE_FIREWORKS, MAX_FINALE_FIREWORKS + 1);
    return;
  }

  if (fireworksSystem.finaleFireworksRemaining == 0) {
    fireworksSystem.finaleActive = false;
    return;
  }

  fireworksSystem.finaleFireworksRemaining--;
  if (fireworksSystem.finaleFireworksRemaining == 0) {
    fireworksSystem.finaleActive = false;
  }
}

constexpr uint16_t SKY_FIREWORK_LAUNCH_WEIGHT =
    CHRYSANTHEMUM_KIND_WEIGHT + WILLOW_KIND_WEIGHT + RING_KIND_WEIGHT +
    SMILEY_KIND_WEIGHT + STAR_KIND_WEIGHT + FIGURE_EIGHT_KIND_WEIGHT +
    HEART_KIND_WEIGHT + BROCADE_KIND_WEIGHT + DIAMOND_KIND_WEIGHT +
    SPIRAL_KIND_WEIGHT + FISH_KIND_WEIGHT + SCREEN_FLASH_KIND_WEIGHT;

constexpr uint16_t FIREWORK_KIND_WEIGHT =
    CHRYSANTHEMUM_KIND_WEIGHT + WILLOW_KIND_WEIGHT + RING_KIND_WEIGHT +
    SMILEY_KIND_WEIGHT + STAR_KIND_WEIGHT + FIGURE_EIGHT_KIND_WEIGHT +
    HEART_KIND_WEIGHT + BROCADE_KIND_WEIGHT + DIAMOND_KIND_WEIGHT +
    SPIRAL_KIND_WEIGHT + FISH_KIND_WEIGHT + SCREEN_FLASH_KIND_WEIGHT;

bool randomLaunchIsGroundComet() {
  uint16_t roll =
      random(0, GROUND_COMET_LAUNCH_WEIGHT + SKY_FIREWORK_LAUNCH_WEIGHT);
  return roll < GROUND_COMET_LAUNCH_WEIGHT;
}

FireworkKind randomFireworkKind() {
  uint16_t roll = random(0, FIREWORK_KIND_WEIGHT);
  if (roll < CHRYSANTHEMUM_KIND_WEIGHT) {
    return FireworkKind::Chrysanthemum;
  }
  roll -= CHRYSANTHEMUM_KIND_WEIGHT;
  if (roll < WILLOW_KIND_WEIGHT) {
    return FireworkKind::Willow;
  }
  roll -= WILLOW_KIND_WEIGHT;
  if (roll < RING_KIND_WEIGHT) {
    return FireworkKind::Ring;
  }
  roll -= RING_KIND_WEIGHT;
  if (roll < SMILEY_KIND_WEIGHT) {
    return FireworkKind::Smiley;
  }
  roll -= SMILEY_KIND_WEIGHT;
  if (roll < STAR_KIND_WEIGHT) {
    return FireworkKind::Star;
  }
  roll -= STAR_KIND_WEIGHT;
  if (roll < FIGURE_EIGHT_KIND_WEIGHT) {
    return FireworkKind::FigureEight;
  }
  roll -= FIGURE_EIGHT_KIND_WEIGHT;
  if (roll < HEART_KIND_WEIGHT) {
    return FireworkKind::Heart;
  }
  roll -= HEART_KIND_WEIGHT;
  if (roll < BROCADE_KIND_WEIGHT) {
    return FireworkKind::Brocade;
  }
  roll -= BROCADE_KIND_WEIGHT;
  if (roll < DIAMOND_KIND_WEIGHT) {
    return FireworkKind::Diamond;
  }
  roll -= DIAMOND_KIND_WEIGHT;
  if (roll < SPIRAL_KIND_WEIGHT) {
    return FireworkKind::Spiral;
  }
  roll -= SPIRAL_KIND_WEIGHT;
  if (roll < FISH_KIND_WEIGHT) {
    return FireworkKind::Fish;
  }
  return FireworkKind::ScreenFlash;
}

GroundCometMode randomGroundCometMode() {
  uint8_t roll = random(0, 3);
  if (roll == 0) {
    return GroundCometMode::LeftToRight;
  }
  if (roll == 1) {
    return GroundCometMode::RightToLeft;
  }
  return GroundCometMode::AllAtOnce;
}

float groundCometXForIndex(uint8_t index, uint8_t count) {
  uint16_t width = matrixWidth();
  if (count == 0 || width <= 1) {
    return 0.0f;
  }
  return (index + 1) * ((width - 1) / (float)(count + 1));
}

void initGroundComet(GroundComet &comet, float x, unsigned long launchMs) {
  comet.scheduled = true;
  comet.launched = false;
  comet.launchMs = launchMs;
  comet.lastFrameMs = launchMs;
  comet.x = x;
  comet.y = 0.0f;
  comet.frame = 0;
  comet.trailCount = 0;
  comet.twinkleOffset = random(0, 3);

  uint16_t width = matrixWidth();
  float center = (width - 1) / 2.0f;
  float normalizedSide = center > 0.0f ? (x - center) / center : 0.0f;
  float angleDeg = fabsf(normalizedSide) * MAX_GROUND_COMET_ANGLE_DEG;
  float angleRad = angleDeg * PI_F / 180.0f;
  comet.vy = GROUND_COMET_VERTICAL_SPEED;
  comet.vx = tanf(angleRad) * comet.vy * (normalizedSide < 0.0f ? -1.0f : 1.0f);
}

void startGroundCometVolley(unsigned long now) {
  if (anyGroundCometScheduled()) {
    return;
  }

  uint8_t maxComets =
      min((uint8_t)MAX_SIMULTANEOUS_GROUND_COMETS, (uint8_t)MAX_GROUND_COMETS);
  uint8_t cometCount = random(MIN_GROUND_COMETS, maxComets + 1);
  GroundCometMode mode = randomGroundCometMode();
  unsigned long launchMs = now;

  for (uint8_t sequence = 0; sequence < cometCount; sequence++) {
    uint8_t cometIndex = sequence;
    if (mode == GroundCometMode::RightToLeft) {
      cometIndex = cometCount - 1 - sequence;
    }

    if (mode != GroundCometMode::AllAtOnce && sequence > 0) {
      launchMs += random(MIN_GROUND_COMET_SEQUENCE_DELAY_MS,
                         MAX_GROUND_COMET_SEQUENCE_DELAY_MS + 1);
    }

    initGroundComet(groundComets[cometIndex],
                    groundCometXForIndex(cometIndex, cometCount), launchMs);
  }
}

void initFirework(FireworkInstance &fw, unsigned long now) {
  fw.active = true;
  fw.kind = randomFireworkKind();
  fw.phase = FireworkPhase::Mortar;
  fw.screenFlashEmbersReady = false;
  fw.mortar = createMortarPath();
  fw.frame = 0;
  fw.burstRotation = randomFloat(0.0f, 2.0f * PI_F);
  fw.burstScale = randomFloat(0.78f, 1.22f);
  fw.ringMajorAxis = randomFloat(3.2f, 4.8f) * fw.burstScale;
  fw.ringMinorAxis = randomFloat(1.3f, 2.4f) * fw.burstScale;
  if (fw.kind == FireworkKind::FigureEight) {
    fw.ringMajorAxis = randomFloat(4.7f, 6.0f) * fw.burstScale;
    fw.ringMinorAxis = randomFloat(2.4f, 3.3f) * fw.burstScale;
  } else if (fw.kind == FireworkKind::Heart) {
    fw.burstRotation = randomFloat(-0.46f, 0.46f);
    if (random(0, 4) == 0) {
      fw.burstRotation = 0.0f;
    }
    fw.ringMajorAxis = randomFloat(4.4f, 5.8f) * fw.burstScale;
    fw.ringMinorAxis = randomFloat(2.8f, 3.7f) * fw.burstScale;
  } else if (fw.kind == FireworkKind::Spiral) {
    fw.ringMajorAxis = randomFloat(6.8f, 9.0f) * fw.burstScale;
    fw.ringMinorAxis = randomFloat(3.6f, 5.0f) * fw.burstScale;
  }
  if (fw.kind != FireworkKind::ScreenFlash) {
    configureChrysanthemumStars(fw);
  }
  if (fw.kind == FireworkKind::Ring) {
    configureRingPointLifetimes(fw);
  }
  fw.lastFrameMs = now;
}

bool advanceFirework(FireworkInstance &fw, unsigned long now,
                     const ProgramConfig &cfg) {
  if (now - fw.lastFrameMs < sanitizedFrameMs(cfg)) {
    return true;
  }

  fw.lastFrameMs = now;
  switch (fw.phase) {
  case FireworkPhase::Mortar:
    if (advanceMortarPath(fw.mortar)) {
      if (isShapedKind(fw.kind)) {
        fw.centerRow = SHAPED_BURST_CENTER_ROW;
        fw.centerCol = clampShapedCenterCol((int16_t)roundf(fw.mortar.x));
      } else {
        fw.centerRow = rowFromAltitude(fw.mortar.targetY);
        fw.centerCol = clampCol((int16_t)roundf(fw.mortar.x));
      }
      fw.fuseEndMs = now + random(MIN_FUSE_DELAY_MS, MAX_FUSE_DELAY_MS + 1);
      fw.phase = FireworkPhase::Fuse;
    }
    break;

  case FireworkPhase::Fuse:
    if (timeReached(now, fw.fuseEndMs)) {
      fw.phase = FireworkPhase::Burst;
      fw.frame = 0;
    }
    break;

  case FireworkPhase::Burst:
    fw.frame++;
    if (fw.kind == FireworkKind::ScreenFlash &&
        screenFlashBurstPhase(fw.frame) == ScreenFlashBurstPhase::Dissolve) {
      initScreenFlashEmbers(fw);
    }
    if (!burstHasLiveContent(fw)) {
      if (fw.kind == FireworkKind::ScreenFlash) {
        fw.screenFlashEmbersReady = false;
        screenFlashEmberCount = 0;
      }
      fw.active = false;
      return false;
    }
    break;
  }

  return fw.active;
}

bool advanceGroundComet(GroundComet &comet, unsigned long now,
                        const ProgramConfig &cfg) {
  if (!comet.scheduled) {
    return false;
  }

  if (!comet.launched) {
    if (!timeReached(now, comet.launchMs)) {
      return false;
    }

    comet.launched = true;
    comet.lastFrameMs = now;
    addGroundCometTrailPoint(comet);
    return true;
  }

  if (now - comet.lastFrameMs < sanitizedFrameMs(cfg)) {
    return false;
  }

  comet.lastFrameMs = now;
  comet.frame++;
  comet.x += comet.vx;
  comet.y += comet.vy;
  comet.vy = max(0.45f, comet.vy - GROUND_COMET_DECEL);

  uint16_t width = matrixWidth();
  if (comet.frame > GROUND_COMET_MAX_FRAMES || comet.y > MATRIX_HEIGHT + 1 ||
      comet.x < -2.0f || comet.x > width + 1.0f) {
    comet.scheduled = false;
    comet.launched = false;
    return true;
  }

  addGroundCometTrailPoint(comet);
  return true;
}

void tryLaunchEffect(unsigned long now, const ProgramConfig &cfg) {
  if (!timeReached(now, fireworksSystem.nextLaunchMs)) {
    return;
  }

  bool launched = false;
  if (randomLaunchIsGroundComet()) {
    if (!anyGroundCometScheduled()) {
      startGroundCometVolley(now);
      launched = true;
    }
  } else {
    FireworkInstance *slot = findFreeSlot();
    if (slot != nullptr) {
      initFirework(*slot, now);
      registerFireworkLaunch();
      launched = true;
    }
  }

  if (!launched) {
    FireworkInstance *slot = findFreeSlot();
    if (slot != nullptr) {
      initFirework(*slot, now);
      registerFireworkLaunch();
    }
  }

  scheduleNextLaunch(now, cfg);
}

void resetAllFireworks() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    fireworks[i].active = false;
    fireworks[i].screenFlashEmbersReady = false;
  }
  screenFlashEmberCount = 0;
}

void resetAllGroundComets() {
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_GROUND_COMETS; i++) {
    groundComets[i].scheduled = false;
    groundComets[i].launched = false;
  }
}
} // namespace

void fireworksStart(const ProgramConfig &cfg) {
  if (!fireworksSystem.seeded) {
    randomSeed(esp_random());
    fireworksSystem.seeded = true;
  }

  unsigned long now = millis();
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  Display.displayClear();
  resetAllFireworks();
  resetAllGroundComets();
  fireworksSystem.nextLaunchMs = now;
  fireworksSystem.finaleActive = false;
  fireworksSystem.finaleFireworksRemaining = 0;
}

void fireworksTick(const ProgramConfig &cfg) {
  unsigned long now = millis();
  tryLaunchEffect(now, cfg);

  bool changed = false;
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_FIREWORKS; i++) {
    if (!fireworks[i].active) {
      continue;
    }

    bool wasActive = fireworks[i].active;
    advanceFirework(fireworks[i], now, cfg);
    if (wasActive != fireworks[i].active || fireworks[i].active) {
      changed = true;
    }
  }
  for (uint8_t i = 0; i < MAX_SIMULTANEOUS_GROUND_COMETS; i++) {
    if (advanceGroundComet(groundComets[i], now, cfg)) {
      changed = true;
    }
  }

  if (changed || anyFireworkActive() || anyGroundCometVisible()) {
    renderAllFireworks(cfg);
  }
}
