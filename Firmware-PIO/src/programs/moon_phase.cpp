#include "moon_phase.h"

#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include <time.h>

namespace {
constexpr uint8_t DISPLAY_HEIGHT = 8;
constexpr uint8_t DISPLAY_WIDTH = 32;
constexpr unsigned long FRAME_MS = 80UL;
constexpr unsigned long NAME_HOLD_MS = 5000UL;
constexpr unsigned long MOON_HOLD_MS = 8000UL;
constexpr unsigned long NTP_RETRY_MS = 30000UL;
constexpr double SYNODIC_MONTH_DAYS = 29.530588853;
// Known new moon near 2000-01-06 18:14 UTC
constexpr double KNOWN_NEW_MOON_JD = 2451550.1;

enum class View : uint8_t { Moon, Name };

struct State {
  View view = View::Moon;
  unsigned long viewStartMs = 0;
  unsigned long lastFrameMs = 0;
  unsigned long lastNtpAttemptMs = 0;
  bool timeSynced = false;
  float phase = 0.0f; // 0=new .. 0.5=full .. 1=new
  char nameBuffer[24];
  uint16_t twinkle = 0;
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

double julianDay(int year, int month, int day, int hour, int minute,
                 int second) {
  int y = year;
  int m = month;
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  int a = y / 100;
  int b = 2 - a + a / 4;
  double dayFraction =
      (hour + minute / 60.0 + second / 3600.0) / 24.0;
  return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + day +
         b - 1524.5 + dayFraction;
}

float moonPhaseFromJulianDay(double jd) {
  double days = std::fmod(jd - KNOWN_NEW_MOON_JD, SYNODIC_MONTH_DAYS);
  if (days < 0.0) {
    days += SYNODIC_MONTH_DAYS;
  }
  return static_cast<float>(days / SYNODIC_MONTH_DAYS);
}

const char *phaseName(float phase) {
  // Eight named phases centered on their peaks.
  float p = phase - std::floor(phase);
  int index = static_cast<int>(std::floor(p * 8.0f + 0.5f)) % 8;
  static const char *names[] = {
      "New Moon",         "Waxing Crescent", "First Quarter", "Waxing Gibbous",
      "Full Moon",        "Waning Gibbous",  "Last Quarter",  "Waning Crescent"};
  return names[index];
}

bool syncTimeIfNeeded(unsigned long now) {
  if (state.timeSynced) {
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (state.lastNtpAttemptMs != 0 &&
      now - state.lastNtpAttemptMs < NTP_RETRY_MS) {
    return false;
  }
  state.lastNtpAttemptMs = now;

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 4000)) {
    return false;
  }
  state.timeSynced = true;
  return true;
}

void updatePhaseFromClockOrDemo(unsigned long now) {
  if (syncTimeIfNeeded(now)) {
    time_t nowSecs = time(nullptr);
    struct tm utc;
    gmtime_r(&nowSecs, &utc);
    double jd = julianDay(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                          utc.tm_hour, utc.tm_min, utc.tm_sec);
    state.phase = moonPhaseFromJulianDay(jd);
  } else {
    // Slow demo cycle (~60s per synodic month visualization)
    state.phase = std::fmod((now / 1000.0f) / 60.0f, 1.0f);
  }
  strncpy(state.nameBuffer, phaseName(state.phase), sizeof(state.nameBuffer) - 1);
  state.nameBuffer[sizeof(state.nameBuffer) - 1] = '\0';
}

void drawStars() {
  // Sparse twinkling field away from the moon.
  static const int8_t starCoords[][2] = {
      {1, 22}, {0, 27}, {2, 30}, {5, 24}, {6, 29}, {3, 20}, {7, 26}, {4, 31}};
  for (uint8_t i = 0; i < sizeof(starCoords) / sizeof(starCoords[0]); i++) {
    if (((state.twinkle + i * 3) / 4) % 5 != 0) {
      setPixel(starCoords[i][0], starCoords[i][1]);
    }
  }
}

void drawMoonDisc(float phase) {
  constexpr float cx = 8.0f;
  constexpr float cy = 3.5f;
  constexpr float radius = 3.4f;
  float p = phase - std::floor(phase);
  constexpr float kPi = 3.14159265f;
  float limb = std::cos(2.0f * kPi * p);
  bool waxing = p <= 0.5f;

  for (int16_t row = 0; row < DISPLAY_HEIGHT; row++) {
    for (int16_t col = 0; col < 16; col++) {
      float u = (static_cast<float>(col) - cx) / radius;
      float v = (static_cast<float>(row) - cy) / radius;
      if (u * u + v * v > 1.0f) {
        continue;
      }
      bool lit = waxing ? (u >= limb) : (u <= -limb);
      if (lit) {
        setPixel(row, col);
      }
    }
  }
}

void renderMoonFrame() {
  beginFrame();
  drawMoonDisc(state.phase);
  drawStars();
  endFrame();
}

void startNameScroll(const ProgramConfig &cfg) {
  Display.displayClear();
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  Display.setTextAlignment(PA_LEFT);
  unsigned int speed = cfg.scrollSpeedMs > 0 ? cfg.scrollSpeedMs : 75U;
  Display.displayScroll(state.nameBuffer, PA_LEFT, PA_SCROLL_LEFT, speed);
}

void enterView(View view, const ProgramConfig &cfg, unsigned long now) {
  state.view = view;
  state.viewStartMs = now;
  if (view == View::Moon) {
    renderMoonFrame();
  } else {
    startNameScroll(cfg);
  }
}
} // namespace

void moonPhaseStart(const ProgramConfig &cfg) {
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  Display.displayClear();
  state.timeSynced = false;
  state.lastNtpAttemptMs = 0;
  state.twinkle = 0;
  state.lastFrameMs = 0;
  unsigned long now = millis();
  updatePhaseFromClockOrDemo(now);
  enterView(View::Moon, cfg, now);
}

void moonPhaseTick(const ProgramConfig &cfg) {
  unsigned long now = millis();
  if (now - state.lastFrameMs < FRAME_MS) {
    if (state.view == View::Name && Display.displayAnimate()) {
      Display.displayReset();
    }
    return;
  }
  state.lastFrameMs = now;
  state.twinkle++;

  updatePhaseFromClockOrDemo(now);

  if (state.view == View::Moon) {
    renderMoonFrame();
    if (now - state.viewStartMs >= MOON_HOLD_MS) {
      enterView(View::Name, cfg, now);
    }
    return;
  }

  // Name scroll view
  if (Display.displayAnimate()) {
    Display.displayReset();
  }
  if (now - state.viewStartMs >= NAME_HOLD_MS) {
    enterView(View::Moon, cfg, now);
  }
}
