#include "card_view.h"

#include "font3x5.h"
#include "../hardware/hub75_profile.h"

#include <cmath>
#include <stdio.h>
#include <string.h>

namespace card {
namespace {

constexpr uint16_t kWidth = flight_info_hardware::kDisplayWidth;
constexpr uint8_t kHeight = flight_info_hardware::kDisplayHeight;
constexpr int kVertRateLevelFpm = 100;
constexpr uint8_t kTextLeft = 3;
constexpr uint8_t kPairGap = 2;
constexpr uint8_t kRow0Y = 0;
constexpr uint8_t kRow1Y = 7;
constexpr uint8_t kRow2Y = 14;
constexpr uint8_t kRow3Y = 21;

constexpr uint8_t kBgR = 2;
constexpr uint8_t kBgG = 6;
constexpr uint8_t kBgB = 14;
constexpr uint8_t kCallsignR = 80;
constexpr uint8_t kCallsignG = 220;
constexpr uint8_t kCallsignB = 255;
constexpr uint8_t kAirlineR = 120;
constexpr uint8_t kAirlineG = 150;
constexpr uint8_t kAirlineB = 190;
constexpr uint8_t kTypeR = 255;
constexpr uint8_t kTypeG = 190;
constexpr uint8_t kTypeB = 70;
constexpr uint8_t kAltR = 230;
constexpr uint8_t kAltG = 230;
constexpr uint8_t kAltB = 230;
constexpr uint8_t kSpeedR = 230;
constexpr uint8_t kSpeedG = 230;
constexpr uint8_t kSpeedB = 230;
constexpr uint8_t kTrackR = 150;
constexpr uint8_t kTrackG = 150;
constexpr uint8_t kTrackB = 160;
constexpr uint8_t kClimbR = 70;
constexpr uint8_t kClimbG = 220;
constexpr uint8_t kClimbB = 110;
constexpr uint8_t kDescR = 240;
constexpr uint8_t kDescG = 80;
constexpr uint8_t kDescB = 80;
constexpr uint8_t kLevelR = 120;
constexpr uint8_t kLevelG = 120;
constexpr uint8_t kLevelB = 130;
constexpr uint8_t kDotR = 60;
constexpr uint8_t kDotG = 70;
constexpr uint8_t kDotB = 90;
constexpr uint8_t kDotOnR = 220;
constexpr uint8_t kDotOnG = 230;
constexpr uint8_t kDotOnB = 255;

uint16_t colorBuffer[kHeight][kWidth];

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
                               (b >> 3));
}

uint8_t scaleChannel(uint8_t value, uint8_t level) {
  const uint16_t scaled = static_cast<uint16_t>(value) * level / 15;
  return scaled == 0 && value > 0 ? 1 : static_cast<uint8_t>(scaled);
}

bool inPanel(int x, int y) {
  return x >= 0 && y >= 0 && x < static_cast<int>(kWidth) &&
         y < static_cast<int>(kHeight);
}

void setPanel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (!inPanel(x, y)) {
    return;
  }
  colorBuffer[y][x] = rgb565(r, g, b);
}

void fillBackground(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t color = rgb565(r, g, b);
  for (uint8_t row = 0; row < kHeight; ++row) {
    for (uint16_t col = 0; col < kWidth; ++col) {
      colorBuffer[row][col] = color;
    }
  }
}

void drawGlyph(int x, int y, char value, uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t *columns = font3x5::glyph(value);
  for (uint8_t col = 0; col < font3x5::kWidth; ++col) {
    const uint8_t bits = columns[col];
    for (uint8_t row = 0; row < font3x5::kHeight; ++row) {
      if (bits & (1 << row)) {
        setPanel(x + col, y + row, r, g, b);
      }
    }
  }
}

int drawText(int x, int y, const char *text, uint8_t r, uint8_t g, uint8_t b,
             int maxChars = 20) {
  if (!text) {
    return x;
  }
  int cursor = x;
  int drawn = 0;
  while (text[0] && drawn < maxChars) {
    if (drawn > 0) {
      cursor += 1;
    }
    drawGlyph(cursor, y, text[0], r, g, b);
    cursor += font3x5::kWidth;
    ++text;
    ++drawn;
  }
  return cursor;
}

int charsThatFit(int pixelWidth) {
  if (pixelWidth < static_cast<int>(font3x5::kWidth)) {
    return 0;
  }
  return (pixelWidth + 1) / static_cast<int>(font3x5::kCellWidth);
}

void drawLeftRight(int y, const char *left, uint8_t lr, uint8_t lg, uint8_t lb,
                   const char *right, uint8_t rr, uint8_t rg, uint8_t rb) {
  const int rightWidth = right ? static_cast<int>(font3x5::textWidth(right)) : 0;
  int rightX = static_cast<int>(kWidth) - rightWidth;
  if (rightX < kTextLeft) {
    rightX = kTextLeft;
  }

  int leftMax = 20;
  if (right && right[0]) {
    leftMax = charsThatFit(rightX - kPairGap - kTextLeft);
    if (leftMax < 0) {
      leftMax = 0;
    }
  }
  if (left && left[0] && leftMax > 0) {
    drawText(kTextLeft, y, left, lr, lg, lb, leftMax);
  }
  if (right && right[0]) {
    const int leftEnd =
        kTextLeft + static_cast<int>(font3x5::textWidth(left)) + kPairGap;
    if (rightX < leftEnd && left && left[0]) {
      const int fitted = charsThatFit(static_cast<int>(kWidth) - leftEnd);
      if (fitted <= 0) {
        return;
      }
      rightX = static_cast<int>(kWidth) -
               static_cast<int>(fitted * font3x5::kWidth + (fitted - 1));
      drawText(rightX, y, right, rr, rg, rb, fitted);
      return;
    }
    drawText(rightX, y, right, rr, rg, rb);
  }
}

void drawAccent(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t y = 0; y < kHeight; ++y) {
    setPanel(0, y, r, g, b);
    setPanel(1, y, r, g, b);
  }
}

void drawDots(int leftBound, uint8_t index, uint8_t count, uint8_t onR,
              uint8_t onG, uint8_t onB, uint8_t offR, uint8_t offG,
              uint8_t offB) {
  if (count == 0) {
    return;
  }
  const int total = static_cast<int>(count) * 2 - 1;
  int x = static_cast<int>(kWidth) - total;
  if (x < leftBound) {
    x = leftBound;
  }
  for (uint8_t i = 0; i < count; ++i) {
    if (x >= static_cast<int>(kWidth)) {
      break;
    }
    const bool on = i == index;
    setPanel(x, 1, on ? onR : offR, on ? onG : offG, on ? onB : offB);
    setPanel(x, 2, on ? onR : offR, on ? onG : offG, on ? onB : offB);
    x += 2;
  }
}

void drawChevron(int x, int y, bool up, uint8_t r, uint8_t g, uint8_t b) {
  if (up) {
    setPanel(x + 1, y, r, g, b);
    setPanel(x, y + 1, r, g, b);
    setPanel(x + 1, y + 1, r, g, b);
    setPanel(x + 2, y + 1, r, g, b);
    setPanel(x, y + 2, r, g, b);
    setPanel(x + 2, y + 2, r, g, b);
  } else {
    setPanel(x, y, r, g, b);
    setPanel(x + 2, y, r, g, b);
    setPanel(x, y + 1, r, g, b);
    setPanel(x + 1, y + 1, r, g, b);
    setPanel(x + 2, y + 1, r, g, b);
    setPanel(x + 1, y + 2, r, g, b);
  }
}

int convertedSpeed(int16_t gsKt, uint8_t speedUnit) {
  if (speedUnit == FLIGHT_SPEED_UNIT_MPH) {
    return static_cast<int>(lroundf(gsKt * 1.15078f));
  }
  if (speedUnit == FLIGHT_SPEED_UNIT_KPH) {
    return static_cast<int>(lroundf(gsKt * 1.852f));
  }
  return gsKt;
}

const char *speedSuffix(uint8_t speedUnit) {
  if (speedUnit == FLIGHT_SPEED_UNIT_MPH) {
    return "mph";
  }
  if (speedUnit == FLIGHT_SPEED_UNIT_KPH) {
    return "kph";
  }
  return "kt";
}

} // namespace

void draw(tinker::RuntimeContext &runtime, const adsb::Aircraft &aircraft,
          uint8_t index, uint8_t count, uint8_t speedUnit, uint8_t brightness) {
  uint8_t level = brightness;
  if (level > 15) {
    level = 15;
  }
  if (level < 1) {
    level = 1;
  }

  fillBackground(scaleChannel(kBgR, level), scaleChannel(kBgG, level),
                 scaleChannel(kBgB, level));

  uint8_t accentR = scaleChannel(kLevelR, level);
  uint8_t accentG = scaleChannel(kLevelG, level);
  uint8_t accentB = scaleChannel(kLevelB, level);
  uint8_t vertR = accentR;
  uint8_t vertG = accentG;
  uint8_t vertB = accentB;
  const char *vertLabel = "LVL";
  bool climbing = false;
  bool descending = false;
  if (aircraft.hasVertRate) {
    if (aircraft.vertRateFpm >= kVertRateLevelFpm) {
      accentR = scaleChannel(kClimbR, level);
      accentG = scaleChannel(kClimbG, level);
      accentB = scaleChannel(kClimbB, level);
      vertR = accentR;
      vertG = accentG;
      vertB = accentB;
      vertLabel = nullptr;
      climbing = true;
    } else if (aircraft.vertRateFpm <= -kVertRateLevelFpm) {
      accentR = scaleChannel(kDescR, level);
      accentG = scaleChannel(kDescG, level);
      accentB = scaleChannel(kDescB, level);
      vertR = accentR;
      vertG = accentG;
      vertB = accentB;
      vertLabel = nullptr;
      descending = true;
    }
  }

  drawAccent(accentR, accentG, accentB);

  const char *callsign =
      aircraft.callsign[0] ? aircraft.callsign : "Aircraft";
  const int callsignEnd = drawText(
      kTextLeft, kRow0Y, callsign, scaleChannel(kCallsignR, level),
      scaleChannel(kCallsignG, level), scaleChannel(kCallsignB, level));
  drawDots(callsignEnd + kPairGap, index, count, scaleChannel(kDotOnR, level),
           scaleChannel(kDotOnG, level), scaleChannel(kDotOnB, level),
           scaleChannel(kDotR, level), scaleChannel(kDotG, level),
           scaleChannel(kDotB, level));

  char airline[12] = "";
  const bool hasAirline =
      adsb::airlineNameFromCallsign(aircraft.callsign, airline, sizeof(airline));
  drawLeftRight(kRow1Y, hasAirline ? airline : "",
                scaleChannel(kAirlineR, level), scaleChannel(kAirlineG, level),
                scaleChannel(kAirlineB, level), aircraft.typeCode,
                scaleChannel(kTypeR, level), scaleChannel(kTypeG, level),
                scaleChannel(kTypeB, level));

  char altText[12];
  snprintf(altText, sizeof(altText), "%ldft",
           static_cast<long>(aircraft.altFt));
  char speedText[12];
  snprintf(speedText, sizeof(speedText), "%d%s",
           convertedSpeed(aircraft.gsKt, speedUnit), speedSuffix(speedUnit));
  drawLeftRight(kRow2Y, altText, scaleChannel(kAltR, level),
                scaleChannel(kAltG, level), scaleChannel(kAltB, level),
                speedText, scaleChannel(kSpeedR, level),
                scaleChannel(kSpeedG, level), scaleChannel(kSpeedB, level));

  char trackText[8];
  snprintf(trackText, sizeof(trackText), "%03ddeg", aircraft.trackDeg);
  char rateText[10] = "";
  if (climbing || descending) {
    snprintf(rateText, sizeof(rateText), "%d",
             aircraft.vertRateFpm < 0 ? -aircraft.vertRateFpm
                                      : aircraft.vertRateFpm);
    const int rateWidth = static_cast<int>(font3x5::textWidth(rateText));
    const int chevronWidth = 4;
    int rateX = static_cast<int>(kWidth) - rateWidth;
    const int leftEnd =
        kTextLeft + static_cast<int>(font3x5::textWidth(trackText)) + kPairGap;
    if (rateX - chevronWidth < leftEnd) {
      rateX = leftEnd + chevronWidth;
    }
    drawText(kTextLeft, kRow3Y, trackText, scaleChannel(kTrackR, level),
             scaleChannel(kTrackG, level), scaleChannel(kTrackB, level));
    drawChevron(rateX - chevronWidth, kRow3Y + 1, climbing, vertR, vertG,
                vertB);
    drawText(rateX, kRow3Y, rateText, vertR, vertG, vertB);
  } else {
    drawLeftRight(kRow3Y, trackText, scaleChannel(kTrackR, level),
                  scaleChannel(kTrackG, level), scaleChannel(kTrackB, level),
                  vertLabel, vertR, vertG, vertB);
  }

  runtime.setBrightness(brightness);
  runtime.blitRgb565(&colorBuffer[0][0], kWidth, kHeight);
}

} // namespace card
