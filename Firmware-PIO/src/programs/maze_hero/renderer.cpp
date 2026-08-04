#include "renderer.h"

#include "../program.h"
#include "play_world.h"

namespace MazeHero {
namespace {

MD_MAX72XX *matrix() { return Display.getGraphicObject(); }

constexpr int16_t CLOSEUP_CELL_PITCH = 10;
constexpr int16_t CLOSEUP_SAMPLE_RADIUS_CELLS = 10;
constexpr int16_t CLOSEUP_WALL_HALF_THICKNESS = 1;
constexpr uint16_t CLOSEUP_START_ZOOM_Q8 = 2U * 256U;
constexpr uint16_t CLOSEUP_HOLD_ZOOM_Q8 = 7U * 256U;

void beginFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  matrix()->clear();
}

void endFrame() {
  matrix()->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix()->update();
}

void setPixel(int16_t row, int16_t col, bool on = true) {
  if (row < 0 || row >= DISPLAY_HEIGHT || col < 0 ||
      col >= matrix()->getColumnCount()) {
    return;
  }
  matrix()->setPoint(row, col, on);
}

void clearPixel(int16_t row, int16_t col) { setPixel(row, col, false); }

int16_t horizontalMargin(const Maze &maze, uint16_t viewWidth) {
  int16_t width = mazePixelWidth(maze);
  return width < viewWidth ? (viewWidth - width) / 2 : 0;
}

int16_t verticalMargin(const Maze &maze, uint8_t viewHeight) {
  int16_t height = mazePixelHeight(maze);
  return height < viewHeight ? (viewHeight - height) / 2 : 0;
}

int16_t screenX(const Maze &maze, const Camera &camera, int16_t worldX,
                uint16_t viewWidth) {
  return worldX - camera.x() + horizontalMargin(maze, viewWidth);
}

int16_t screenY(const Maze &maze, const Camera &camera, int16_t worldY,
                uint8_t viewHeight) {
  return worldY - camera.y() + verticalMargin(maze, viewHeight);
}

int16_t screenXQ8(const Maze &maze, const CameraRenderPosition &camera,
                  int32_t worldXQ8, uint16_t viewWidth);
int16_t screenYQ8(const Maze &maze, const CameraRenderPosition &camera,
                  int32_t worldYQ8, uint8_t viewHeight);

void drawWorldPixel(const Maze &maze, const Camera &camera, int16_t worldY,
                    int16_t worldX, uint16_t viewWidth, uint8_t viewHeight) {
  setPixel(screenY(maze, camera, worldY, viewHeight),
           screenX(maze, camera, worldX, viewWidth));
}

void drawHorizontalWall(const Maze &maze, const Camera &camera, int16_t worldY,
                        int16_t centerX, uint16_t viewWidth,
                        uint8_t viewHeight) {
  for (int16_t x = centerX - 1; x <= centerX + 1; x++) {
    drawWorldPixel(maze, camera, worldY, x, viewWidth, viewHeight);
  }
}

void drawVerticalWall(const Maze &maze, const Camera &camera, int16_t worldX,
                      int16_t centerY, uint16_t viewWidth,
                      uint8_t viewHeight) {
  for (int16_t y = centerY - 1; y <= centerY + 1; y++) {
    drawWorldPixel(maze, camera, y, worldX, viewWidth, viewHeight);
  }
}

void drawDiscoveredWalls(const Maze &maze, const FogOfWar &fog,
                         const Camera &camera, Coord coord,
                         uint16_t viewWidth, uint8_t viewHeight) {
  if (!fog.isDiscovered(maze, coord)) {
    return;
  }

  int16_t centerX = cellCenterX(coord);
  int16_t centerY = cellCenterY(coord);

  if (maze.hasWall(coord, Direction::North)) {
    drawHorizontalWall(maze, camera, centerY - 1, centerX, viewWidth,
                       viewHeight);
  }
  if (maze.hasWall(coord, Direction::East)) {
    drawVerticalWall(maze, camera, centerX + 1, centerY, viewWidth, viewHeight);
  }
  if (maze.hasWall(coord, Direction::South)) {
    drawHorizontalWall(maze, camera, centerY + 1, centerX, viewWidth,
                       viewHeight);
  }
  if (maze.hasWall(coord, Direction::West)) {
    drawVerticalWall(maze, camera, centerX - 1, centerY, viewWidth, viewHeight);
  }
}

void drawWorldPixelQ8(const Maze &maze, const CameraRenderPosition &camera,
                      int16_t worldY, int16_t worldX, uint16_t viewWidth,
                      uint8_t viewHeight) {
  setPixel(screenYQ8(maze, camera, (int32_t)worldY * 256L, viewHeight),
           screenXQ8(maze, camera, (int32_t)worldX * 256L, viewWidth));
}

void drawHorizontalWallQ8(const Maze &maze,
                          const CameraRenderPosition &camera, int16_t worldY,
                          int16_t centerX, uint16_t viewWidth,
                          uint8_t viewHeight) {
  for (int16_t x = centerX - 1; x <= centerX + 1; x++) {
    drawWorldPixelQ8(maze, camera, worldY, x, viewWidth, viewHeight);
  }
}

void drawVerticalWallQ8(const Maze &maze, const CameraRenderPosition &camera,
                        int16_t worldX, int16_t centerY, uint16_t viewWidth,
                        uint8_t viewHeight) {
  for (int16_t y = centerY - 1; y <= centerY + 1; y++) {
    drawWorldPixelQ8(maze, camera, y, worldX, viewWidth, viewHeight);
  }
}

void drawDiscoveredWallsQ8(const Maze &maze, const FogOfWar &fog,
                           const CameraRenderPosition &camera, Coord coord,
                           uint16_t viewWidth, uint8_t viewHeight) {
  if (!fog.isDiscovered(maze, coord)) {
    return;
  }

  int16_t centerX = cellCenterX(coord);
  int16_t centerY = cellCenterY(coord);

  if (maze.hasWall(coord, Direction::North)) {
    drawHorizontalWallQ8(maze, camera, centerY - 1, centerX, viewWidth,
                         viewHeight);
  }
  if (maze.hasWall(coord, Direction::East)) {
    drawVerticalWallQ8(maze, camera, centerX + 1, centerY, viewWidth,
                       viewHeight);
  }
  if (maze.hasWall(coord, Direction::South)) {
    drawHorizontalWallQ8(maze, camera, centerY + 1, centerX, viewWidth,
                         viewHeight);
  }
  if (maze.hasWall(coord, Direction::West)) {
    drawVerticalWallQ8(maze, camera, centerX - 1, centerY, viewWidth,
                       viewHeight);
  }
}

uint8_t progressQ8(unsigned long elapsedMs, unsigned long durationMs) {
  if (durationMs == 0 || elapsedMs >= durationMs) {
    return 255;
  }
  return (uint8_t)((elapsedMs * 255UL) / durationMs);
}

uint8_t easeInOutQ8(uint8_t value) {
  uint32_t t = value;
  return (uint8_t)((t * t * (765UL - 2UL * t) + 32512UL) / 65025UL);
}

uint16_t lerpQ8(uint16_t from, uint16_t to, uint8_t progress) {
  int32_t delta = (int32_t)to - (int32_t)from;
  return (uint16_t)((int32_t)from + (delta * progress) / 255L);
}

uint16_t zoomQ8FromPercent(uint16_t percent) {
  if (percent < 100U) {
    percent = 100U;
  }
  return (uint16_t)(((uint32_t)CLOSEUP_START_ZOOM_Q8 * percent + 50U) / 100U);
}

int16_t closeupCellCenterX(Coord coord) {
  return (int16_t)coord.x * CLOSEUP_CELL_PITCH;
}

int16_t closeupCellCenterY(Coord coord) {
  return (int16_t)coord.y * CLOSEUP_CELL_PITCH;
}

bool insideRange(int16_t value, int16_t start, int16_t end) {
  return value >= start && value <= end;
}

int16_t maxInt16(int16_t a, int16_t b) { return a > b ? a : b; }

int16_t minInt16(int16_t a, int16_t b) { return a < b ? a : b; }

int16_t lerpInt16(int16_t from, int16_t to, uint8_t progress) {
  int32_t delta = (int32_t)to - (int32_t)from;
  return (int16_t)((int32_t)from + (delta * progress) / 255L);
}

int32_t lerpInt32(int32_t from, int32_t to, uint8_t progress) {
  return from + ((to - from) * progress) / 255L;
}

int16_t q8ToInt16(int32_t valueQ8) { return (int16_t)(valueQ8 / 256L); }

int16_t q8ToNearestInt16(int32_t valueQ8) {
  if (valueQ8 >= 0) {
    return (int16_t)((valueQ8 + 128L) / 256L);
  }
  return (int16_t)-((-valueQ8 + 128L) / 256L);
}

int16_t oldMazePixelToCloseup(int16_t oldWorld) {
  return ((oldWorld - 1) * CLOSEUP_CELL_PITCH) / 2;
}

int32_t oldMazePixelQ8ToCloseupQ8(int32_t oldWorldQ8) {
  return ((oldWorldQ8 - 256L) * CLOSEUP_CELL_PITCH) / 2L;
}

int16_t screenXQ8(const Maze &maze, const CameraRenderPosition &camera,
                  int32_t worldXQ8, uint16_t viewWidth) {
  int32_t screenQ8 = worldXQ8 - camera.xQ8 +
                     (int32_t)horizontalMargin(maze, viewWidth) * 256L;
  return q8ToNearestInt16(screenQ8);
}

int16_t screenYQ8(const Maze &maze, const CameraRenderPosition &camera,
                  int32_t worldYQ8, uint8_t viewHeight) {
  int32_t screenQ8 = worldYQ8 - camera.yQ8 +
                     (int32_t)verticalMargin(maze, viewHeight) * 256L;
  return q8ToNearestInt16(screenQ8);
}

int16_t playCameraFocusX(const Maze &maze, const Camera &camera,
                         uint16_t viewWidth) {
  int16_t oldWorldX =
      camera.x() + (int16_t)(viewWidth / 2) - horizontalMargin(maze, viewWidth);
  return oldMazePixelToCloseup(oldWorldX);
}

int16_t playCameraFocusY(const Maze &maze, const Camera &camera,
                         uint8_t viewHeight) {
  int16_t oldWorldY =
      camera.y() + (int16_t)(viewHeight / 2) - verticalMargin(maze, viewHeight);
  return oldMazePixelToCloseup(oldWorldY);
}

int32_t playCameraFocusXQ8(const Maze &maze,
                           const CameraRenderPosition &camera,
                           uint16_t viewWidth) {
  int32_t oldWorldXQ8 = camera.xQ8 + (int32_t)(viewWidth / 2) * 256L -
                        (int32_t)horizontalMargin(maze, viewWidth) * 256L;
  return oldMazePixelQ8ToCloseupQ8(oldWorldXQ8);
}

int32_t playCameraFocusYQ8(const Maze &maze,
                           const CameraRenderPosition &camera,
                           uint8_t viewHeight) {
  int32_t oldWorldYQ8 = camera.yQ8 + (int32_t)(viewHeight / 2) * 256L -
                        (int32_t)verticalMargin(maze, viewHeight) * 256L;
  return oldMazePixelQ8ToCloseupQ8(oldWorldYQ8);
}

Coord focusCoord(const Maze &maze, int16_t focusX, int16_t focusY) {
  int16_t x = (focusX + CLOSEUP_CELL_PITCH / 2) / CLOSEUP_CELL_PITCH;
  int16_t y = (focusY + CLOSEUP_CELL_PITCH / 2) / CLOSEUP_CELL_PITCH;
  x = maxInt16(0, minInt16((int16_t)maze.width() - 1, x));
  y = maxInt16(0, minInt16((int16_t)maze.height() - 1, y));
  return {(uint8_t)x, (uint8_t)y};
}

int16_t worldQ8ToScreen(int32_t worldQ8, int32_t focusQ8, uint16_t zoomQ8,
                        int16_t center2) {
  int32_t doubledScreenDelta =
      ((worldQ8 - focusQ8) * zoomQ8) / (CLOSEUP_CELL_PITCH * 32768L);
  return (doubledScreenDelta + center2 + 1) / 2;
}

void fillScreenRect(int16_t left, int16_t top, int16_t right, int16_t bottom,
                    uint16_t width, uint8_t height) {
  left = maxInt16(0, left);
  top = maxInt16(0, top);
  right = minInt16((int16_t)width - 1, right);
  bottom = minInt16((int16_t)height - 1, bottom);
  if (left > right || top > bottom) {
    return;
  }

  for (int16_t row = top; row <= bottom; row++) {
    for (int16_t col = left; col <= right; col++) {
      setPixel(row, col);
    }
  }
}

void drawCloseupWallRect(int16_t minWorldX, int16_t minWorldY,
                         int16_t maxWorldX, int16_t maxWorldY,
                         int32_t focusXQ8, int32_t focusYQ8, uint16_t zoomQ8,
                         uint16_t width, uint8_t height) {
  int16_t centerCol2 = (int16_t)width - 1;
  int16_t centerRow2 = (int16_t)height - 1;
  int16_t left =
      worldQ8ToScreen((int32_t)minWorldX * 256L, focusXQ8, zoomQ8, centerCol2);
  int16_t right =
      worldQ8ToScreen((int32_t)maxWorldX * 256L, focusXQ8, zoomQ8, centerCol2);
  int16_t top =
      worldQ8ToScreen((int32_t)minWorldY * 256L, focusYQ8, zoomQ8, centerRow2);
  int16_t bottom =
      worldQ8ToScreen((int32_t)maxWorldY * 256L, focusYQ8, zoomQ8, centerRow2);
  if (left > right) {
    int16_t tmp = left;
    left = right;
    right = tmp;
  }
  if (top > bottom) {
    int16_t tmp = top;
    top = bottom;
    bottom = tmp;
  }
  fillScreenRect(left, top, right, bottom, width, height);
}

void drawCloseupCellWalls(const Maze &maze, Coord coord, int32_t focusXQ8,
                          int32_t focusYQ8, uint16_t zoomQ8, uint16_t width,
                          uint8_t height) {
  int16_t centerX = closeupCellCenterX(coord);
  int16_t centerY = closeupCellCenterY(coord);
  int16_t halfPitch = CLOSEUP_CELL_PITCH / 2;
  int16_t wall = CLOSEUP_WALL_HALF_THICKNESS;

  if (maze.hasWall(coord, Direction::North)) {
    drawCloseupWallRect(centerX - halfPitch, centerY - halfPitch - wall,
                        centerX + halfPitch, centerY - halfPitch + wall,
                        focusXQ8, focusYQ8, zoomQ8, width, height);
  }
  if (maze.hasWall(coord, Direction::East)) {
    drawCloseupWallRect(centerX + halfPitch - wall, centerY - halfPitch,
                        centerX + halfPitch + wall, centerY + halfPitch,
                        focusXQ8, focusYQ8, zoomQ8, width, height);
  }
  if (maze.hasWall(coord, Direction::South)) {
    drawCloseupWallRect(centerX - halfPitch, centerY + halfPitch - wall,
                        centerX + halfPitch, centerY + halfPitch + wall,
                        focusXQ8, focusYQ8, zoomQ8, width, height);
  }
  if (maze.hasWall(coord, Direction::West)) {
    drawCloseupWallRect(centerX - halfPitch - wall, centerY - halfPitch,
                        centerX - halfPitch + wall, centerY + halfPitch,
                        focusXQ8, focusYQ8, zoomQ8, width, height);
  }
}

void drawCloseupMaze(const Maze &maze, const FogOfWar &fog, int32_t focusXQ8,
                     int32_t focusYQ8, uint16_t zoomQ8, uint16_t width,
                     uint8_t height) {
  Coord center = focusCoord(maze, q8ToInt16(focusXQ8), q8ToInt16(focusYQ8));
  int16_t startY = maxInt16(0, (int16_t)center.y - CLOSEUP_SAMPLE_RADIUS_CELLS);
  int16_t endY = minInt16((int16_t)maze.height() - 1,
                          (int16_t)center.y + CLOSEUP_SAMPLE_RADIUS_CELLS);
  int16_t startX = maxInt16(0, (int16_t)center.x - CLOSEUP_SAMPLE_RADIUS_CELLS);
  int16_t endX = minInt16((int16_t)maze.width() - 1,
                          (int16_t)center.x + CLOSEUP_SAMPLE_RADIUS_CELLS);

  for (int16_t y = startY; y <= endY; y++) {
    for (int16_t x = startX; x <= endX; x++) {
      Coord coord = {(uint8_t)x, (uint8_t)y};
      if (!fog.isDiscovered(maze, coord)) {
        continue;
      }
      drawCloseupCellWalls(maze, coord, focusXQ8, focusYQ8, zoomQ8, width,
                           height);
    }
  }
}

bool playZoomFrameVisible(const PlayZoomFrame &zoomFrame) {
  unsigned long totalMs =
      zoomFrame.zoomInMs + zoomFrame.holdMs + zoomFrame.zoomOutMs;
  return zoomFrame.active && totalMs > 0 && zoomFrame.elapsedMs < totalMs;
}

bool inCircle(int16_t row, int16_t col, int16_t centerRow, int16_t centerCol,
              int16_t radius) {
  if (radius <= 0) {
    return row == centerRow && col == centerCol;
  }
  int32_t dx = col - centerCol;
  int32_t dy = row - centerRow;
  int32_t radius2 = (int32_t)radius * radius;
  return dx * dx + dy * dy <= radius2;
}

Direction exitDirection(const Maze &maze, Coord hero) {
  if (hero.y == 0) {
    return Direction::North;
  }
  if (hero.x == maze.width() - 1) {
    return Direction::East;
  }
  if (hero.y == maze.height() - 1) {
    return Direction::South;
  }
  return Direction::West;
}

void drawHeroAtScreen(int16_t centerCol, int16_t centerRow) {
  for (int16_t row = centerRow - 1; row <= centerRow + 1; row++) {
    for (int16_t col = centerCol - 1; col <= centerCol + 1; col++) {
      if (inCircle(row, col, centerRow, centerCol, 1)) {
        setPixel(row, col);
      }
    }
  }
}

void drawHeroAt(uint16_t width, uint8_t height, int16_t offsetX,
                int16_t offsetY) {
  drawHeroAtScreen(width / 2 + offsetX, height / 2 + offsetY);
}

void drawZoomedScene(const Maze &maze, const FogOfWar &fog,
                     const ZoomView &view, int32_t heroFocusXQ8,
                     int32_t heroFocusYQ8, uint16_t width, uint8_t height) {
  drawCloseupMaze(maze, fog, view.focusXQ8, view.focusYQ8, view.zoomQ8, width,
                  height);
  int16_t heroCol =
      worldQ8ToScreen(heroFocusXQ8, view.focusXQ8, view.zoomQ8,
                      (int16_t)width - 1);
  int16_t heroRow =
      worldQ8ToScreen(heroFocusYQ8, view.focusYQ8, view.zoomQ8,
                      (int16_t)height - 1);
  drawHeroAtScreen(heroCol, heroRow);
}

void drawExitingHero(const Maze &maze, Coord hero, uint16_t width,
                     uint8_t height, uint8_t progress) {
  Direction dir = exitDirection(maze, hero);
  uint8_t eased = easeInOutQ8(progress);
  int16_t exitDistance =
      (dir == Direction::East || dir == Direction::West) ? width / 2 + 4
                                                         : height / 2 + 3;
  int16_t travel = ((int32_t)exitDistance * eased) / 255;
  drawHeroAt(width, height, directionDx(dir) * travel,
             directionDy(dir) * travel);
}

void drawEnteringHero(const Maze &maze, Coord hero, uint16_t width,
                      uint8_t height, uint8_t progress) {
  Direction dir = exitDirection(maze, hero);
  uint8_t eased = easeInOutQ8(progress);
  int16_t enterDistance =
      (dir == Direction::East || dir == Direction::West) ? width / 2 + 4
                                                         : height / 2 + 3;
  int16_t travel = ((int32_t)enterDistance * (255 - eased)) / 255;
  drawHeroAt(width, height, directionDx(dir) * travel,
             directionDy(dir) * travel);
}

} // namespace

uint16_t Renderer::viewWidth() const { return matrix()->getColumnCount(); }

ZoomView Renderer::playZoomView(const Maze &maze,
                                const CameraRenderPosition &cameraRender,
                                int32_t heroRenderWorldXQ8,
                                int32_t heroRenderWorldYQ8,
                                const PlayZoomFrame &zoomFrame) const {
  uint16_t width = viewWidth();
  uint8_t height = viewHeight();
  ZoomView normalView = {playCameraFocusXQ8(maze, cameraRender, width),
                         playCameraFocusYQ8(maze, cameraRender, height),
                         CLOSEUP_START_ZOOM_Q8};

  if (!playZoomFrameVisible(zoomFrame)) {
    return normalView;
  }

  int32_t heroFocusXQ8 = oldMazePixelQ8ToCloseupQ8(heroRenderWorldXQ8);
  int32_t heroFocusYQ8 = oldMazePixelQ8ToCloseupQ8(heroRenderWorldYQ8);
  uint16_t targetZoomQ8 = zoomQ8FromPercent(zoomFrame.targetZoomPercent);
  unsigned long zoomOutStartMs = zoomFrame.zoomInMs + zoomFrame.holdMs;

  if (zoomFrame.elapsedMs < zoomFrame.zoomInMs) {
    uint8_t eased =
        easeInOutQ8(progressQ8(zoomFrame.elapsedMs, zoomFrame.zoomInMs));
    return {lerpInt32(normalView.focusXQ8, heroFocusXQ8, eased),
            lerpInt32(normalView.focusYQ8, heroFocusYQ8, eased),
            lerpQ8(CLOSEUP_START_ZOOM_Q8, targetZoomQ8, eased)};
  }

  if (zoomFrame.elapsedMs < zoomOutStartMs) {
    return {heroFocusXQ8, heroFocusYQ8, targetZoomQ8};
  }

  unsigned long zoomOutElapsed = zoomFrame.elapsedMs - zoomOutStartMs;
  uint8_t eased =
      easeInOutQ8(progressQ8(zoomOutElapsed, zoomFrame.zoomOutMs));
  return {lerpInt32(heroFocusXQ8, normalView.focusXQ8, eased),
          lerpInt32(heroFocusYQ8, normalView.focusYQ8, eased),
          lerpQ8(targetZoomQ8, CLOSEUP_START_ZOOM_Q8, eased)};
}

void Renderer::renderPlaying(const Maze &maze, const FogOfWar &fog,
                             const CameraRenderPosition &cameraRender,
                             int32_t heroRenderWorldXQ8,
                             int32_t heroRenderWorldYQ8,
                             const PlayZoomFrame &zoomFrame,
                             uint8_t frame) const {
  (void)frame;
  uint16_t width = viewWidth();
  uint8_t height = viewHeight();
  beginFrame();

  if (playZoomFrameVisible(zoomFrame)) {
    ZoomView view = playZoomView(maze, cameraRender, heroRenderWorldXQ8,
                                 heroRenderWorldYQ8, zoomFrame);
    drawZoomedScene(maze, fog, view,
                    oldMazePixelQ8ToCloseupQ8(heroRenderWorldXQ8),
                    oldMazePixelQ8ToCloseupQ8(heroRenderWorldYQ8), width,
                    height);
  } else {
    for (uint8_t y = 0; y < maze.height(); y++) {
      for (uint8_t x = 0; x < maze.width(); x++) {
        Coord coord = {x, y};
        drawDiscoveredWallsQ8(maze, fog, cameraRender, coord, width, height);
      }
    }

    int16_t heroRow =
        screenYQ8(maze, cameraRender, heroRenderWorldYQ8, height);
    int16_t heroCol =
        screenXQ8(maze, cameraRender, heroRenderWorldXQ8, width);
    setPixel(heroRow, heroCol);
  }

  endFrame();
}

void Renderer::renderIntro(const Maze &maze, const FogOfWar &fog,
                           const Camera &camera, Coord hero,
                           unsigned long elapsedMs, uint8_t frame) const {
  (void)frame;
  uint16_t width = viewWidth();
  uint8_t height = viewHeight();
  int16_t heroFocusX = closeupCellCenterX(hero);
  int16_t heroFocusY = closeupCellCenterY(hero);

  beginFrame();
  if (elapsedMs < INTRO_PAUSE_MS) {
    drawCloseupMaze(maze, fog, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, CLOSEUP_HOLD_ZOOM_Q8, width,
                    height);
    drawEnteringHero(maze, hero, width, height, 0);
  } else if (elapsedMs < INTRO_PAUSE_MS + INTRO_ENTER_MS) {
    unsigned long enterElapsed = elapsedMs - INTRO_PAUSE_MS;
    drawCloseupMaze(maze, fog, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, CLOSEUP_HOLD_ZOOM_Q8, width,
                    height);
    drawEnteringHero(maze, hero, width, height,
                     progressQ8(enterElapsed, INTRO_ENTER_MS));
  } else {
    unsigned long zoomElapsed = elapsedMs - INTRO_PAUSE_MS - INTRO_ENTER_MS;
    uint8_t eased = easeInOutQ8(progressQ8(zoomElapsed, INTRO_ZOOM_OUT_MS));
    int16_t endFocusX = playCameraFocusX(maze, camera, width);
    int16_t endFocusY = playCameraFocusY(maze, camera, height);
    ZoomView view = {lerpInt32((int32_t)heroFocusX * 256L,
                               (int32_t)endFocusX * 256L, eased),
                     lerpInt32((int32_t)heroFocusY * 256L,
                               (int32_t)endFocusY * 256L, eased),
                     lerpQ8(CLOSEUP_HOLD_ZOOM_Q8, CLOSEUP_START_ZOOM_Q8,
                            eased)};
    drawZoomedScene(maze, fog, view, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, width, height);
  }
  endFrame();
}

void Renderer::renderVictory(const Maze &maze, const FogOfWar &fog,
                             const ZoomView &startView, Coord hero,
                             unsigned long elapsedMs, uint8_t frame) const {
  (void)frame;
  uint16_t width = viewWidth();
  uint8_t height = viewHeight();
  int16_t heroFocusX = closeupCellCenterX(hero);
  int16_t heroFocusY = closeupCellCenterY(hero);

  beginFrame();
  if (elapsedMs < VICTORY_ZOOM_IN_MS) {
    uint8_t eased = easeInOutQ8(progressQ8(elapsedMs, VICTORY_ZOOM_IN_MS));
    ZoomView view = {lerpInt32(startView.focusXQ8,
                               (int32_t)heroFocusX * 256L, eased),
                     lerpInt32(startView.focusYQ8,
                               (int32_t)heroFocusY * 256L, eased),
                     lerpQ8(startView.zoomQ8, CLOSEUP_HOLD_ZOOM_Q8, eased)};
    drawZoomedScene(maze, fog, view, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, width, height);
  } else if (elapsedMs < VICTORY_ZOOM_IN_MS + VICTORY_HOLD_MS) {
    ZoomView view = {heroFocusX, heroFocusY, CLOSEUP_HOLD_ZOOM_Q8};
    drawZoomedScene(maze, fog, view, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, width, height);
  } else {
    unsigned long exitElapsed =
        elapsedMs - VICTORY_ZOOM_IN_MS - VICTORY_HOLD_MS;
    drawCloseupMaze(maze, fog, (int32_t)heroFocusX * 256L,
                    (int32_t)heroFocusY * 256L, CLOSEUP_HOLD_ZOOM_Q8, width,
                    height);
    drawExitingHero(maze, hero, width, height,
                    progressQ8(exitElapsed, VICTORY_EXIT_MS));
  }
  endFrame();
}

} // namespace MazeHero
