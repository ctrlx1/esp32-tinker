#include "pixel_art_program.h"

#include "generated/pixel_art_catalog.h"

#include <MD_MAX72xx.h>
#include <esp_system.h>
#include <pgmspace.h>

namespace PixelArt {
namespace {

constexpr uint8_t DISPLAY_HEIGHT = 8;
constexpr uint8_t ART_WIDTH = 32;
constexpr unsigned long SCROLL_STEP_MS = 90UL;
constexpr unsigned long FAST_SCROLL_STEP_MS = 32UL;
constexpr unsigned long DISSOLVE_FRAME_MS = 25UL;
constexpr unsigned long IMAGE_HOLD_MS = 800UL;
constexpr uint16_t DISSOLVE_PIXEL_COUNT = DISPLAY_HEIGHT * ART_WIDTH;
constexpr uint8_t DISSOLVE_PIXELS_PER_FRAME = 16;
constexpr uint16_t DISSOLVE_STEP = 37;

enum class Phase : uint8_t {
  HoldingTop,
  SlowDown,
  FastUp,
  FastDown,
  Dissolving
};

struct State {
  size_t imageIndex = 0;
  bool seeded = false;
  uint16_t topRow = 0;
  unsigned long lastStepMs = 0;
  unsigned long holdStartMs = 0;
  uint16_t dissolveStep = 0;
  Phase phase = Phase::HoldingTop;
};

State state;

MD_MAX72XX *matrix() { return Display.getGraphicObject(); }

uint8_t sanitizedBrightness(uint8_t brightness) {
  return brightness > 15 ? 15 : brightness;
}

const Image &currentImage() { return IMAGES[state.imageIndex]; }

size_t randomImageIndex() {
  if (IMAGE_COUNT <= 1) {
    return 0;
  }

  size_t nextIndex = random(static_cast<long>(IMAGE_COUNT - 1));
  if (nextIndex >= state.imageIndex) {
    nextIndex++;
  }
  return nextIndex;
}

uint16_t maxTopRow(const Image &image) {
  return image.height > DISPLAY_HEIGHT ? image.height - DISPLAY_HEIGHT : 0;
}

void beginUpdate() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
}

void beginFrame() {
  beginUpdate();
  matrix()->clear();
}

void endFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix()->update();
}

void renderImageWindow(const Image &image, uint16_t topRow) {
  beginFrame();
  uint16_t matrixWidth = matrix()->getColumnCount();
  uint8_t drawWidth = matrixWidth < ART_WIDTH ? matrixWidth : ART_WIDTH;

  for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
    uint16_t sourceRow = topRow + row;
    if (sourceRow >= image.height) {
      continue;
    }

    uint32_t rowBits = pgm_read_dword(&image.rows[sourceRow]);
    for (uint8_t col = 0; col < drawWidth; col++) {
      if ((rowBits & (1UL << col)) != 0) {
        matrix()->setPoint(row, col, true);
      }
    }
  }

  endFrame();
}

void clearDisplay() {
  beginFrame();
  endFrame();
}

void startImage(unsigned long now) {
  state.topRow = 0;
  state.lastStepMs = now;
  state.holdStartMs = now;
  state.phase = Phase::HoldingTop;
  renderImageWindow(currentImage(), state.topRow);
}

void advanceToNextImage(unsigned long now) {
  state.imageIndex = randomImageIndex();
  startImage(now);
}

bool holdComplete(unsigned long now) {
  return now - state.holdStartMs >= IMAGE_HOLD_MS;
}

void startFastUp(unsigned long now) {
  state.phase = Phase::FastUp;
  state.lastStepMs = now;
}

void startFastDown(unsigned long now) {
  state.phase = Phase::FastDown;
  state.lastStepMs = now;
}

void startDissolve(unsigned long now) {
  state.phase = Phase::Dissolving;
  state.lastStepMs = now;
  state.dissolveStep = 0;
}

void updateHold(unsigned long now) {
  if (!holdComplete(now)) {
    return;
  }

  if (maxTopRow(currentImage()) == 0) {
    startDissolve(now);
    return;
  }

  state.phase = Phase::SlowDown;
  state.lastStepMs = now;
}

void updateSlowDown(unsigned long now) {
  if (now - state.lastStepMs < SCROLL_STEP_MS) {
    return;
  }

  state.lastStepMs = now;
  const Image &image = currentImage();
  uint16_t targetTopRow = maxTopRow(image);
  if (state.topRow < targetTopRow) {
    state.topRow++;
    renderImageWindow(image, state.topRow);
  }

  if (state.topRow >= targetTopRow) {
    startFastUp(now);
  }
}

void updateFastUp(unsigned long now) {
  if (now - state.lastStepMs < FAST_SCROLL_STEP_MS) {
    return;
  }

  state.lastStepMs = now;
  if (state.topRow > 0) {
    state.topRow--;
    renderImageWindow(currentImage(), state.topRow);
  }

  if (state.topRow == 0) {
    startFastDown(now);
  }
}

void updateFastDown(unsigned long now) {
  if (now - state.lastStepMs < FAST_SCROLL_STEP_MS) {
    return;
  }

  state.lastStepMs = now;
  const Image &image = currentImage();
  uint16_t targetTopRow = maxTopRow(image);
  if (state.topRow < targetTopRow) {
    state.topRow++;
    renderImageWindow(image, state.topRow);
  }

  if (state.topRow >= targetTopRow) {
    startDissolve(now);
  }
}

void clearDissolvePixel(uint16_t index) {
  uint16_t permutedIndex = (index * DISSOLVE_STEP) % DISSOLVE_PIXEL_COUNT;
  uint8_t row = permutedIndex / ART_WIDTH;
  uint8_t col = permutedIndex % ART_WIDTH;
  if (col < matrix()->getColumnCount()) {
    matrix()->setPoint(row, col, false);
  }
}

void updateDissolve(unsigned long now) {
  if (now - state.lastStepMs < DISSOLVE_FRAME_MS) {
    return;
  }

  state.lastStepMs = now;
  beginUpdate();
  for (uint8_t i = 0; i < DISSOLVE_PIXELS_PER_FRAME &&
                      state.dissolveStep < DISSOLVE_PIXEL_COUNT;
       i++) {
    clearDissolvePixel(state.dissolveStep);
    state.dissolveStep++;
  }
  endFrame();

  if (state.dissolveStep >= DISSOLVE_PIXEL_COUNT) {
    clearDisplay();
    advanceToNextImage(now);
  }
}

} // namespace

void start(const ProgramConfig &cfg) {
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  if (IMAGE_COUNT == 0 || IMAGES == nullptr) {
    clearDisplay();
    return;
  }

  if (!state.seeded) {
    randomSeed(esp_random());
    state.seeded = true;
  }

  state.imageIndex = random(static_cast<long>(IMAGE_COUNT));
  startImage(millis());
}

void tick(const ProgramConfig &cfg) {
  (void)cfg;
  if (IMAGE_COUNT == 0 || IMAGES == nullptr) {
    return;
  }

  unsigned long now = millis();
  if (state.phase == Phase::SlowDown) {
    updateSlowDown(now);
    return;
  }
  if (state.phase == Phase::FastUp) {
    updateFastUp(now);
    return;
  }
  if (state.phase == Phase::FastDown) {
    updateFastDown(now);
    return;
  }
  if (state.phase == Phase::Dissolving) {
    updateDissolve(now);
    return;
  }

  updateHold(now);
}

} // namespace PixelArt
