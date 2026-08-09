#include "moon_phase.h"

#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace {
constexpr uint8_t DISPLAY_HEIGHT = 8;
constexpr uint8_t DISPLAY_WIDTH = 32;
constexpr uint8_t MOON_CANVAS_HEIGHT = DISPLAY_WIDTH; // full-width disc, taller canvas
constexpr uint8_t MOON_DIAMETER = DISPLAY_WIDTH;
constexpr size_t SCROLL_BUFFER_SIZE = 128;
constexpr unsigned long FRAME_MS = 80UL;
constexpr unsigned long PAN_STEP_MS = 100UL;
constexpr unsigned long PAN_HOLD_MS = 900UL;
constexpr unsigned long NTP_RETRY_MS = 30000UL;
constexpr double SYNODIC_MONTH_DAYS = 29.530588853;
// Known new moon near 2000-01-06 18:14 UTC
constexpr double KNOWN_NEW_MOON_JD = 2451550.1;

enum class View : uint8_t { Moon, Name };
enum class PanPhase : uint8_t {
  HoldingTop,
  ScrollingDown,
  HoldingBottom,
  ScrollingUp
};

struct State {
  View view = View::Moon;
  PanPhase panPhase = PanPhase::HoldingTop;
  unsigned long viewStartMs = 0;
  unsigned long lastFrameMs = 0;
  unsigned long lastPanStepMs = 0;
  unsigned long lastNtpAttemptMs = 0;
  bool timeSynced = false;
  float phase = 0.0f; // 0=new .. 0.5=full .. 1=new
  uint8_t illuminationPercent = 0;
  uint8_t topRow = 0;
  char *scrollBuffer = nullptr;
};

uint8_t maxTopRow() {
  return MOON_CANVAS_HEIGHT > DISPLAY_HEIGHT
             ? static_cast<uint8_t>(MOON_CANVAS_HEIGHT - DISPLAY_HEIGHT)
             : 0;
}

State state;

bool ensureScrollBuffer() {
  if (state.scrollBuffer != nullptr) {
    return true;
  }
  state.scrollBuffer =
      static_cast<char *>(malloc(SCROLL_BUFFER_SIZE));
  if (state.scrollBuffer != nullptr) {
    state.scrollBuffer[0] = '\0';
  }
  return state.scrollBuffer != nullptr;
}

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

float normalizePhase(float phase) {
  float p = phase - std::floor(phase);
  if (p < 0.0f) {
    p += 1.0f;
  }
  return p;
}

const char *phaseName(float phase) {
  // Eight named phases centered on their peaks.
  float p = normalizePhase(phase);
  int index = static_cast<int>(std::floor(p * 8.0f + 0.5f)) % 8;
  static const char *names[] = {
      "New Moon",         "Waxing Crescent", "First Quarter", "Waxing Gibbous",
      "Full Moon",        "Waning Gibbous",  "Last Quarter",  "Waning Crescent"};
  return names[index];
}

uint8_t illuminationPercent(float phase) {
  constexpr float kPi = 3.14159265f;
  float p = normalizePhase(phase);
  float fraction = 0.5f * (1.0f - std::cos(2.0f * kPi * p));
  int percent = static_cast<int>(std::lround(fraction * 100.0f));
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 100) {
    percent = 100;
  }
  return static_cast<uint8_t>(percent);
}

// Days until the next occurrence of targetPhase in [0,1).
double daysUntilPhase(float currentPhase, float targetPhase) {
  float current = normalizePhase(currentPhase);
  float target = normalizePhase(targetPhase);
  float delta = target - current;
  if (delta <= 0.0001f) {
    delta += 1.0f;
  }
  return static_cast<double>(delta) * SYNODIC_MONTH_DAYS;
}

void appendDuration(char *out, size_t outSize, double days) {
  if (days < 0.0) {
    days = 0.0;
  }
  long totalMinutes = static_cast<long>(std::lround(days * 24.0 * 60.0));
  if (totalMinutes < 1) {
    totalMinutes = 1;
  }

  long wholeDays = totalMinutes / (24L * 60L);
  long hours = (totalMinutes % (24L * 60L)) / 60L;
  long minutes = totalMinutes % 60L;

  char piece[24];
  if (wholeDays > 0) {
    snprintf(piece, sizeof(piece), "%ldd %ldh", wholeDays, hours);
  } else if (hours > 0) {
    snprintf(piece, sizeof(piece), "%ldh %ldm", hours, minutes);
  } else {
    snprintf(piece, sizeof(piece), "%ldm", minutes);
  }

  size_t used = strlen(out);
  if (used + 1 >= outSize) {
    return;
  }
  strncat(out, piece, outSize - used - 1);
}

void appendUpcoming(char *out, size_t outSize, const char *label,
                    float currentPhase, float targetPhase) {
  size_t used = strlen(out);
  if (used + 3 >= outSize) {
    return;
  }
  strncat(out, " | ", outSize - used - 1);
  used = strlen(out);
  strncat(out, label, outSize - used - 1);
  used = strlen(out);
  strncat(out, " in ", outSize - used - 1);
  appendDuration(out, outSize, daysUntilPhase(currentPhase, targetPhase));
}

void buildScrollText() {
  if (!ensureScrollBuffer()) {
    return;
  }
  // Example:
  // Full Moon 87% | New 3d 14h | 1st Qtr 11d 2h | Full 18d 8h | Last Qtr 25d 20h
  snprintf(state.scrollBuffer, SCROLL_BUFFER_SIZE, "%s %u%%",
           phaseName(state.phase),
           static_cast<unsigned>(state.illuminationPercent));
  appendUpcoming(state.scrollBuffer, SCROLL_BUFFER_SIZE, "New", state.phase,
                 0.0f);
  appendUpcoming(state.scrollBuffer, SCROLL_BUFFER_SIZE, "1st Qtr", state.phase,
                 0.25f);
  appendUpcoming(state.scrollBuffer, SCROLL_BUFFER_SIZE, "Full", state.phase,
                 0.5f);
  appendUpcoming(state.scrollBuffer, SCROLL_BUFFER_SIZE, "Last Qtr",
                 state.phase, 0.75f);
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
  state.phase = normalizePhase(state.phase);
  state.illuminationPercent = illuminationPercent(state.phase);
}

bool moonPixelLit(int16_t canvasRow, int16_t col, float phase) {
  constexpr float cx = (DISPLAY_WIDTH - 1) / 2.0f; // 15.5
  constexpr float cy = (MOON_CANVAS_HEIGHT - 1) / 2.0f;
  constexpr float radius = (MOON_DIAMETER - 1) / 2.0f;
  float p = normalizePhase(phase);
  constexpr float kPi = 3.14159265f;
  float limb = std::cos(2.0f * kPi * p);
  bool waxing = p <= 0.5f;

  float u = (static_cast<float>(col) - cx) / radius;
  float v = (static_cast<float>(canvasRow) - cy) / radius;
  if (u * u + v * v > 1.0f) {
    return false;
  }
  return waxing ? (u >= limb) : (u <= -limb);
}

void renderMoonFrame() {
  beginFrame();
  for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
    int16_t canvasRow = static_cast<int16_t>(state.topRow) + row;
    for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
      if (moonPixelLit(canvasRow, col, state.phase)) {
        setPixel(row, col);
      }
    }
  }
  endFrame();
}

void resetMoonPan(unsigned long now) {
  state.panPhase = PanPhase::HoldingTop;
  state.topRow = 0;
  state.lastPanStepMs = now;
  state.viewStartMs = now;
}

void startNameScroll(const ProgramConfig &cfg) {
  buildScrollText();
  Display.displayClear();
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  Display.setTextAlignment(PA_LEFT);
  unsigned int speed = cfg.scrollSpeedMs > 0 ? cfg.scrollSpeedMs : 75U;
  const char *text =
      (state.scrollBuffer != nullptr && state.scrollBuffer[0] != '\0')
          ? state.scrollBuffer
          : phaseName(state.phase);
  Display.displayScroll(text, PA_LEFT, PA_SCROLL_LEFT, speed);
}

void enterView(View view, const ProgramConfig &cfg, unsigned long now) {
  state.view = view;
  state.viewStartMs = now;
  if (view == View::Moon) {
    resetMoonPan(now);
    renderMoonFrame();
  } else {
    startNameScroll(cfg);
  }
}

// Returns true when one full top->bottom->top pan cycle is complete.
bool updateMoonPan(unsigned long now) {
  const uint8_t bottom = maxTopRow();

  switch (state.panPhase) {
  case PanPhase::HoldingTop:
    if (now - state.lastPanStepMs >= PAN_HOLD_MS) {
      state.panPhase = PanPhase::ScrollingDown;
      state.lastPanStepMs = now;
    }
    break;

  case PanPhase::ScrollingDown:
    if (now - state.lastPanStepMs < PAN_STEP_MS) {
      break;
    }
    state.lastPanStepMs = now;
    if (state.topRow < bottom) {
      state.topRow++;
    }
    if (state.topRow >= bottom) {
      state.panPhase = PanPhase::HoldingBottom;
      state.lastPanStepMs = now;
    }
    break;

  case PanPhase::HoldingBottom:
    if (now - state.lastPanStepMs >= PAN_HOLD_MS) {
      state.panPhase = PanPhase::ScrollingUp;
      state.lastPanStepMs = now;
    }
    break;

  case PanPhase::ScrollingUp:
    if (now - state.lastPanStepMs < PAN_STEP_MS) {
      break;
    }
    state.lastPanStepMs = now;
    if (state.topRow > 0) {
      state.topRow--;
    }
    if (state.topRow == 0) {
      return true; // completed one vertical cycle
    }
    break;
  }

  return false;
}
} // namespace

void moonPhaseStart(const ProgramConfig &cfg) {
  Display.setIntensity(sanitizedBrightness(cfg.brightness));
  Display.displayClear();
  state.timeSynced = false;
  state.lastNtpAttemptMs = 0;
  state.lastFrameMs = 0;
  unsigned long now = millis();
  updatePhaseFromClockOrDemo(now);
  enterView(View::Moon, cfg, now);
}

void moonPhaseTick(const ProgramConfig &cfg) {
  unsigned long now = millis();

  // Keep Parola scrolling smooth; switch back only after one full pass.
  if (state.view == View::Name) {
    updatePhaseFromClockOrDemo(now);
    if (Display.displayAnimate()) {
      enterView(View::Moon, cfg, now);
    }
    return;
  }

  if (now - state.lastFrameMs < FRAME_MS) {
    return;
  }
  state.lastFrameMs = now;

  updatePhaseFromClockOrDemo(now);
  const bool cycleDone = updateMoonPan(now);
  renderMoonFrame();
  if (cycleDone) {
    enterView(View::Name, cfg, now);
  }
}
