#include "welcome_screen.h"

#include "wolf_moon.h"

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <esp_system.h>

#include <cstdlib>
#include <cstring>

namespace {

constexpr uint8_t kWidth = 64;
constexpr uint8_t kHeight = 32;
constexpr unsigned long kFrameMs = 33;
constexpr uint8_t kFadeFrames = 30;
constexpr unsigned long kHoldMs = 5000;
constexpr unsigned long kTitleMs = 6000;
constexpr unsigned long kSilverMs = 600;
constexpr unsigned long kSheenMs = 1200;
constexpr uint8_t kSilverR = 176;
constexpr uint8_t kSilverG = 186;
constexpr uint8_t kSilverB = 204;
constexpr uint8_t kSheenRadius = 2;
constexpr uint8_t kMaxLetterPixels = 40;
constexpr uint16_t kCenterX = 31;
constexpr uint16_t kCenterY = 15;
constexpr uint8_t kDissolveBatch = 12;

struct TitleChar {
  char glyph;
  int16_t x;
  int16_t y;
};

struct LetterPixels {
  uint16_t index[kMaxLetterPixels];
  uint8_t count;
};

uint16_t packRgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) |
                               (blue >> 3));
}

void unpackRgb565(uint16_t pixel, uint8_t &red, uint8_t &green, uint8_t &blue) {
  red = static_cast<uint8_t>(((pixel >> 11) & 0x1F) << 3);
  green = static_cast<uint8_t>(((pixel >> 5) & 0x3F) << 2);
  blue = static_cast<uint8_t>((pixel & 0x1F) << 3);
}

uint8_t scaleChannel(uint8_t value, uint8_t level) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * level) / 255);
}

void pace(unsigned long &nextFrameMs) {
  const unsigned long now = millis();
  if (now < nextFrameMs) {
    delay(nextFrameMs - now);
  }
  nextFrameMs = millis() + kFrameMs;
}

void presentBuffer(tinker::RuntimeContext &runtime, const uint16_t *pixels) {
  runtime.beginColorFrame();
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint8_t col = 0; col < kWidth; ++col) {
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      unpackRgb565(pixels[static_cast<size_t>(row) * kWidth + col], red, green,
                   blue);
      runtime.setPixelColor(row, col, red, green, blue);
    }
  }
  runtime.endColorFrame();
}

void presentImage(tinker::RuntimeContext &runtime, uint8_t level) {
  runtime.beginColorFrame();
  for (uint8_t row = 0; row < kWolfMoonHeight; ++row) {
    for (uint8_t col = 0; col < kWolfMoonWidth; ++col) {
      const uint16_t pixel = pgm_read_word(
          &kWolfMoon[static_cast<size_t>(row) * kWolfMoonWidth + col]);
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      unpackRgb565(pixel, red, green, blue);
      runtime.setPixelColor(row, col, scaleChannel(red, level),
                            scaleChannel(green, level),
                            scaleChannel(blue, level));
    }
  }
  runtime.endColorFrame();
}

void fadeImage(tinker::RuntimeContext &runtime, bool fadeIn) {
  unsigned long nextFrameMs = millis();
  for (uint8_t frame = 0; frame < kFadeFrames; ++frame) {
    pace(nextFrameMs);
    const uint8_t step = fadeIn ? frame : static_cast<uint8_t>(kFadeFrames - 1 - frame);
    const uint8_t level =
        static_cast<uint8_t>((static_cast<uint16_t>(step) * 255) / (kFadeFrames - 1));
    presentImage(runtime, level);
  }
}

uint8_t layoutTitle(TitleChar *chars) {
  const char *lines[] = {"Phases", "Of The", "Moon"};
  const int16_t tops[] = {4, 12, 20};
  uint8_t count = 0;
  for (uint8_t line = 0; line < 3; ++line) {
    const uint8_t length = static_cast<uint8_t>(strlen(lines[line]));
    int16_t x = static_cast<int16_t>((kWidth - length * 6) / 2);
    for (uint8_t i = 0; i < length; ++i) {
      chars[count++] = {lines[line][i], x, tops[line]};
      x = static_cast<int16_t>(x + 6);
    }
  }
  return count;
}

void paintSilver(GFXcanvas16 &canvas, const TitleChar *chars, uint8_t count) {
  canvas.fillScreen(0);
  const uint16_t color = packRgb565(kSilverR, kSilverG, kSilverB);
  for (uint8_t i = 0; i < count; ++i) {
    if (chars[i].glyph == ' ') {
      continue;
    }
    canvas.drawChar(chars[i].x, chars[i].y, chars[i].glyph, color, color, 1);
  }
}

void collectLetterPixels(const uint16_t *frame, const TitleChar *chars,
                         uint8_t count, LetterPixels *letters) {
  for (uint8_t i = 0; i < count; ++i) {
    letters[i].count = 0;
    if (chars[i].glyph == ' ') {
      continue;
    }
    for (int8_t dy = 0; dy < 8; ++dy) {
      for (int8_t dx = 0; dx < 6; ++dx) {
        const int x = chars[i].x + dx;
        const int y = chars[i].y + dy;
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
          continue;
        }
        const uint16_t index = static_cast<uint16_t>(y * kWidth + x);
        if (frame[index] == 0 || letters[i].count >= kMaxLetterPixels) {
          continue;
        }
        letters[i].index[letters[i].count++] = index;
      }
    }
  }
}

void sheenBounds(const LetterPixels *letters, uint8_t count, int &minDiag,
                 int &maxDiag) {
  minDiag = kWidth + kHeight;
  maxDiag = 0;
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t p = 0; p < letters[i].count; ++p) {
      const uint16_t index = letters[i].index[p];
      const int diag = static_cast<int>(index % kWidth) +
                       static_cast<int>(index / kWidth);
      if (diag < minDiag) {
        minDiag = diag;
      }
      if (diag > maxDiag) {
        maxDiag = diag;
      }
    }
  }
}

void applySheen(uint16_t *frame, const LetterPixels *letters, uint8_t count,
                unsigned long elapsed) {
  int minDiag = 0;
  int maxDiag = 0;
  sheenBounds(letters, count, minDiag, maxDiag);
  const int start = minDiag - kSheenRadius;
  const int end = maxDiag + kSheenRadius;
  const int span = end - start;
  if (span <= 0) {
    return;
  }
  const int center =
      start + static_cast<int>((elapsed * static_cast<unsigned long>(span)) /
                               kSheenMs);
  const uint16_t sheen = packRgb565(255, 255, 255);
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t p = 0; p < letters[i].count; ++p) {
      const uint16_t index = letters[i].index[p];
      const int diag = static_cast<int>(index % kWidth) +
                       static_cast<int>(index / kWidth);
      const int delta = diag > center ? diag - center : center - diag;
      if (delta <= kSheenRadius) {
        frame[index] = sheen;
      }
    }
  }
}

void applySparkle(uint16_t *frame, const LetterPixels *letters, uint8_t count) {
  const uint16_t spark = packRgb565(255, 255, 255);
  for (uint8_t i = 0; i < count; ++i) {
    if (letters[i].count == 0) {
      continue;
    }
    const uint8_t pick = static_cast<uint8_t>(random(letters[i].count));
    frame[letters[i].index[pick]] = spark;
  }
}

bool anyLit(const uint16_t *pixels) {
  for (size_t i = 0; i < static_cast<size_t>(kWidth) * kHeight; ++i) {
    if (pixels[i] != 0) {
      return true;
    }
  }
  return false;
}

void playDissolve(tinker::RuntimeContext &runtime, uint16_t *frame,
                  uint16_t *scratch) {
  uint16_t count = 0;
  for (uint16_t i = 0; i < static_cast<uint16_t>(kWidth * kHeight); ++i) {
    if (frame[i] != 0) {
      scratch[count++] = i;
    }
  }
  for (uint16_t i = count; i > 1; --i) {
    const uint16_t swapWith = static_cast<uint16_t>(random(i));
    const uint16_t index = scratch[i - 1];
    scratch[i - 1] = scratch[swapWith];
    scratch[swapWith] = index;
  }
  unsigned long nextFrameMs = millis();
  for (uint16_t cursor = 0; cursor < count; cursor =
           static_cast<uint16_t>(cursor + kDissolveBatch)) {
    const uint16_t end = static_cast<uint16_t>(
        cursor + kDissolveBatch > count ? count : cursor + kDissolveBatch);
    for (uint16_t i = cursor; i < end; ++i) {
      frame[scratch[i]] = 0;
    }
    pace(nextFrameMs);
    presentBuffer(runtime, frame);
    if (end == count) {
      break;
    }
  }
}

void playSlide(tinker::RuntimeContext &runtime, uint16_t *frame,
               uint16_t *scratch) {
  const int direction = static_cast<int>(random(4));
  unsigned long nextFrameMs = millis();
  while (anyLit(frame)) {
    memset(scratch, 0, static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    for (uint8_t row = 0; row < kHeight; ++row) {
      for (uint8_t col = 0; col < kWidth; ++col) {
        const uint16_t pixel =
            frame[static_cast<size_t>(row) * kWidth + col];
        if (pixel == 0) {
          continue;
        }
        int nextX = col;
        int nextY = row;
        if (direction == 0) {
          --nextY;
        } else if (direction == 1) {
          ++nextY;
        } else if (direction == 2) {
          --nextX;
        } else {
          ++nextX;
        }
        if (nextX < 0 || nextY < 0 || nextX >= kWidth || nextY >= kHeight) {
          continue;
        }
        scratch[static_cast<size_t>(nextY) * kWidth + nextX] = pixel;
      }
    }
    memcpy(frame, scratch,
           static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    pace(nextFrameMs);
    presentBuffer(runtime, frame);
  }
}

void playWipe(tinker::RuntimeContext &runtime, uint16_t *frame) {
  const int kind = static_cast<int>(random(3));
  unsigned long nextFrameMs = millis();
  if (kind == 2) {
    for (int step = 0; step <= 32; ++step) {
      for (uint8_t row = 0; row < kHeight; ++row) {
        for (uint8_t col = 0; col < kWidth; ++col) {
          const int dx = col > kCenterX ? col - kCenterX : kCenterX - col;
          const int dy = row > kCenterY ? row - kCenterY : kCenterY - row;
          if ((dx > dy ? dx : dy) <= step) {
            frame[static_cast<size_t>(row) * kWidth + col] = 0;
          }
        }
      }
      pace(nextFrameMs);
      presentBuffer(runtime, frame);
    }
    return;
  }
  const uint8_t limit = kind == 0 ? kHeight : kWidth;
  for (uint8_t step = 0; step < limit; ++step) {
    if (kind == 0) {
      memset(frame + static_cast<size_t>(step) * kWidth, 0,
             kWidth * sizeof(uint16_t));
    } else {
      for (uint8_t row = 0; row < kHeight; ++row) {
        frame[static_cast<size_t>(row) * kWidth + step] = 0;
      }
    }
    pace(nextFrameMs);
    presentBuffer(runtime, frame);
  }
}

void playImplode(tinker::RuntimeContext &runtime, uint16_t *frame,
                 uint16_t *scratch) {
  unsigned long nextFrameMs = millis();
  for (uint8_t guard = 0; guard < 40 && anyLit(frame); ++guard) {
    memset(scratch, 0, static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    for (uint8_t row = 0; row < kHeight; ++row) {
      for (uint8_t col = 0; col < kWidth; ++col) {
        const uint16_t pixel =
            frame[static_cast<size_t>(row) * kWidth + col];
        if (pixel == 0) {
          continue;
        }
        int nextX = col;
        int nextY = row;
        if (col < kCenterX) {
          ++nextX;
        } else if (col > kCenterX) {
          --nextX;
        }
        if (row < kCenterY) {
          ++nextY;
        } else if (row > kCenterY) {
          --nextY;
        }
        if (nextX == kCenterX && nextY == kCenterY) {
          continue;
        }
        scratch[static_cast<size_t>(nextY) * kWidth + nextX] = pixel;
      }
    }
    memcpy(frame, scratch,
           static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    pace(nextFrameMs);
    presentBuffer(runtime, frame);
  }
}

void playDrop(tinker::RuntimeContext &runtime, uint16_t *frame,
              uint16_t *scratch) {
  uint8_t speed[kWidth];
  uint8_t wait[kWidth];
  for (uint8_t col = 0; col < kWidth; ++col) {
    speed[col] = static_cast<uint8_t>(1 + random(2));
    wait[col] = static_cast<uint8_t>(random(6));
  }
  unsigned long nextFrameMs = millis();
  while (anyLit(frame)) {
    memset(scratch, 0, static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    bool hold[kWidth];
    for (uint8_t col = 0; col < kWidth; ++col) {
      hold[col] = wait[col] > 0;
      if (wait[col] > 0) {
        --wait[col];
      }
    }
    for (uint8_t row = 0; row < kHeight; ++row) {
      for (uint8_t col = 0; col < kWidth; ++col) {
        const uint16_t pixel =
            frame[static_cast<size_t>(row) * kWidth + col];
        if (pixel == 0) {
          continue;
        }
        if (hold[col]) {
          scratch[static_cast<size_t>(row) * kWidth + col] = pixel;
          continue;
        }
        const int nextY = static_cast<int>(row) + speed[col];
        if (nextY >= kHeight) {
          continue;
        }
        scratch[static_cast<size_t>(nextY) * kWidth + col] = pixel;
      }
    }
    memcpy(frame, scratch,
           static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t));
    pace(nextFrameMs);
    presentBuffer(runtime, frame);
  }
}

void clearPanel(tinker::RuntimeContext &runtime) {
  runtime.beginColorFrame();
  runtime.fillColor(0, 0, 0);
  runtime.endColorFrame();
}

} // namespace

void runWelcome(tinker::RuntimeContext &runtime) {
  if (!runtime.hasColor()) {
    return;
  }

  fadeImage(runtime, true);
  delay(kHoldMs);
  fadeImage(runtime, false);

  TitleChar chars[16];
  const uint8_t count = layoutTitle(chars);
  GFXcanvas16 canvas(kWidth, kHeight);
  uint16_t *frame = canvas.getBuffer();
  if (frame == nullptr) {
    clearPanel(runtime);
    return;
  }

  LetterPixels letters[16];
  paintSilver(canvas, chars, count);
  collectLetterPixels(frame, chars, count, letters);
  randomSeed(esp_random());

  const unsigned long titleStart = millis();
  unsigned long nextFrameMs = titleStart;
  bool sparkleOn = false;
  while (millis() - titleStart < kTitleMs) {
    pace(nextFrameMs);
    const unsigned long elapsed = millis() - titleStart;
    paintSilver(canvas, chars, count);
    if (elapsed >= kSilverMs && elapsed < kSilverMs + kSheenMs) {
      applySheen(frame, letters, count, elapsed - kSilverMs);
    } else if (elapsed >= kSilverMs + kSheenMs) {
      if (sparkleOn) {
        applySparkle(frame, letters, count);
      }
      sparkleOn = !sparkleOn;
    }
    presentBuffer(runtime, frame);
  }
  paintSilver(canvas, chars, count);
  presentBuffer(runtime, frame);

  uint16_t *scratch = static_cast<uint16_t *>(
      malloc(static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t)));
  if (scratch == nullptr) {
    clearPanel(runtime);
    return;
  }
  switch (random(5)) {
  case 0:
    playDissolve(runtime, frame, scratch);
    break;
  case 1:
    playSlide(runtime, frame, scratch);
    break;
  case 2:
    playWipe(runtime, frame);
    break;
  case 3:
    playImplode(runtime, frame, scratch);
    break;
  default:
    playDrop(runtime, frame, scratch);
    break;
  }
  free(scratch);
  clearPanel(runtime);
}
