#include "transitions.h"

#include "program.h"

#include <esp_system.h>

namespace {

enum class TransitionKind : uint8_t { ColumnWipe, SparkleDissolve, CurtainClose };

constexpr unsigned long TRANSITION_FRAME_MS = 35UL;
constexpr uint8_t TRANSITION_HEIGHT = 8;
constexpr uint8_t SPARKLE_FRAMES = 32;
constexpr uint8_t SPARKLE_PIXELS_PER_FRAME = 8;

struct TransitionState {
  bool active = false;
  bool seeded = false;
  TransitionKind kind = TransitionKind::ColumnWipe;
  unsigned long lastFrameMs = 0;
  uint16_t step = 0;
};

TransitionState transitionState;

MD_MAX72XX *matrix() { return Display.getGraphicObject(); }

uint16_t matrixWidth() { return matrix()->getColumnCount(); }

void beginFrame() { matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF); }

void endFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix()->update();
}

void setColumn(uint16_t col, bool on) {
  if (col >= matrixWidth()) {
    return;
  }

  for (uint8_t row = 0; row < TRANSITION_HEIGHT; row++) {
    matrix()->setPoint(row, col, on);
  }
}

void clearDisplay() {
  beginFrame();
  matrix()->clear();
  endFrame();
}

bool readyForNextFrame(unsigned long now) {
  return transitionState.lastFrameMs == 0 ||
         now - transitionState.lastFrameMs >= TRANSITION_FRAME_MS;
}

bool tickColumnWipe() {
  uint16_t width = matrixWidth();
  if (transitionState.step >= width) {
    clearDisplay();
    return true;
  }

  beginFrame();
  setColumn(transitionState.step, false);
  transitionState.step++;
  endFrame();
  return false;
}

bool tickSparkleDissolve() {
  uint16_t width = matrixWidth();
  if (transitionState.step >= SPARKLE_FRAMES || width == 0) {
    clearDisplay();
    return true;
  }

  beginFrame();
  for (uint8_t i = 0; i < SPARKLE_PIXELS_PER_FRAME; i++) {
    uint8_t row = random(TRANSITION_HEIGHT);
    uint16_t col = random(width);
    matrix()->setPoint(row, col, false);
  }
  transitionState.step++;
  endFrame();
  return false;
}

bool tickCurtainClose() {
  uint16_t width = matrixWidth();
  uint16_t halfSteps = (width + 1) / 2;
  if (transitionState.step >= halfSteps) {
    clearDisplay();
    return true;
  }

  uint16_t left = transitionState.step;
  uint16_t right = width - 1 - transitionState.step;
  beginFrame();
  setColumn(left, false);
  setColumn(right, false);
  transitionState.step++;
  endFrame();
  return false;
}

bool tickCurrentTransition() {
  switch (transitionState.kind) {
  case TransitionKind::SparkleDissolve:
    return tickSparkleDissolve();
  case TransitionKind::CurtainClose:
    return tickCurtainClose();
  case TransitionKind::ColumnWipe:
  default:
    return tickColumnWipe();
  }
}

} // namespace

void transitionStartRandom() {
  if (!transitionState.seeded) {
    randomSeed(esp_random());
    transitionState.seeded = true;
  }

  transitionState.active = true;
  transitionState.kind = (TransitionKind)random(3);
  transitionState.lastFrameMs = 0;
  transitionState.step = 0;
}

bool transitionTick() {
  if (!transitionState.active) {
    return true;
  }

  unsigned long now = millis();
  if (!readyForNextFrame(now)) {
    return false;
  }

  transitionState.lastFrameMs = now;
  bool done = tickCurrentTransition();
  if (done) {
    transitionState.active = false;
  }
  return done;
}
