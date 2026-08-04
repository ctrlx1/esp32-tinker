#include "camera.h"

#include "play_world.h"

namespace MazeHero {
namespace {

constexpr uint8_t HERO_VIEW_PADDING_PX = 2;

int32_t worldQ8(int16_t value) { return (int32_t)value * 256L; }

uint8_t progressQ8(unsigned long elapsedMs, unsigned long durationMs) {
  if (durationMs == 0 || elapsedMs >= durationMs) {
    return 255;
  }
  return (uint8_t)((elapsedMs * 255UL) / durationMs);
}

int32_t lerpWorldQ8(int16_t from, int16_t to, uint8_t progress) {
  return worldQ8(from) + ((int32_t)(to - from) * 256L * progress) / 255L;
}

} // namespace

void Camera::reset(const Maze &maze, uint16_t viewWidth, uint8_t viewHeight,
                   Coord hero) {
  offsetX = 0;
  offsetY = 0;
  targetX = 0;
  targetY = 0;
  updateTarget(maze, viewWidth, viewHeight, hero);
  offsetX = targetX;
  offsetY = targetY;
  renderStartX = offsetX;
  renderStartY = offsetY;
  renderEndX = offsetX;
  renderEndY = offsetY;
  renderStartMs = 0;
  renderDurationMs = 1;
  renderActive = false;
}

void Camera::updateTarget(const Maze &maze, uint16_t viewWidth,
                          uint8_t viewHeight, Coord hero) {
  int16_t heroX = cellCenterX(hero);
  int16_t heroY = cellCenterY(hero);

  if (viewWidth > HERO_VIEW_PADDING_PX * 2) {
    int16_t screenX = heroX - targetX;
    if (screenX >= (int16_t)viewWidth - HERO_VIEW_PADDING_PX) {
      targetX = heroX - ((int16_t)viewWidth - HERO_VIEW_PADDING_PX - 1);
    } else if (screenX < HERO_VIEW_PADDING_PX) {
      targetX = heroX - HERO_VIEW_PADDING_PX;
    }
  }

  if (viewHeight > HERO_VIEW_PADDING_PX * 2) {
    int16_t screenY = heroY - targetY;
    if (screenY >= (int16_t)viewHeight - HERO_VIEW_PADDING_PX) {
      targetY = heroY - ((int16_t)viewHeight - HERO_VIEW_PADDING_PX - 1);
    } else if (screenY < HERO_VIEW_PADDING_PX) {
      targetY = heroY - HERO_VIEW_PADDING_PX;
    }
  }

  targetX = clampX(maze, viewWidth, targetX);
  targetY = clampY(maze, viewHeight, targetY);
}

bool Camera::stepTowardTarget(unsigned long now, unsigned long durationMs) {
  int16_t startX = offsetX;
  int16_t startY = offsetY;
  int16_t nextX = approachOnePixel(offsetX, targetX);
  int16_t nextY = approachOnePixel(offsetY, targetY);
  bool changed = nextX != offsetX || nextY != offsetY;
  offsetX = nextX;
  offsetY = nextY;
  renderStartX = startX;
  renderStartY = startY;
  renderEndX = nextX;
  renderEndY = nextY;
  renderStartMs = now;
  renderDurationMs = durationMs > 0 ? durationMs : 1UL;
  renderActive = changed;
  return changed;
}

CameraRenderPosition Camera::renderPosition(unsigned long now) const {
  if (!renderActive) {
    return {worldQ8(offsetX), worldQ8(offsetY)};
  }

  unsigned long elapsedMs = now - renderStartMs;
  if (elapsedMs >= renderDurationMs) {
    return {worldQ8(renderEndX), worldQ8(renderEndY)};
  }

  uint8_t progress = progressQ8(elapsedMs, renderDurationMs);
  return {lerpWorldQ8(renderStartX, renderEndX, progress),
          lerpWorldQ8(renderStartY, renderEndY, progress)};
}

int16_t Camera::clampX(const Maze &maze, uint16_t viewWidth,
                       int16_t value) const {
  int16_t mazeWidth = mazePixelWidth(maze);
  int16_t maxOffset = mazeWidth > viewWidth ? mazeWidth - viewWidth : 0;
  if (value < 0) {
    return 0;
  }
  if (value > maxOffset) {
    return maxOffset;
  }
  return value;
}

int16_t Camera::clampY(const Maze &maze, uint8_t viewHeight,
                       int16_t value) const {
  int16_t mazeHeight = mazePixelHeight(maze);
  int16_t maxOffset = mazeHeight > viewHeight ? mazeHeight - viewHeight : 0;
  if (value < 0) {
    return 0;
  }
  if (value > maxOffset) {
    return maxOffset;
  }
  return value;
}

} // namespace MazeHero
