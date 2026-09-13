#include "moon_phase.h"

#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace {
constexpr uint8_t DISPLAY_HEIGHT = 8;
constexpr uint8_t DISPLAY_WIDTH = 32;
constexpr uint8_t MOON_DIAMETER = 32;
constexpr uint8_t MOON_CANVAS_HEIGHT = MOON_DIAMETER;
constexpr unsigned long FRAME_MS = 80UL;
constexpr unsigned long PAN_STEP_MS = 100UL;
constexpr unsigned long PAN_HOLD_MS = 900UL;
constexpr unsigned long NTP_RETRY_MS = 30000UL;
constexpr time_t MIN_VALID_TIME = 1609459200;
constexpr double SYNODIC_MONTH_DAYS = 29.530588853;
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
  unsigned long lastFrameMs = 0;
  unsigned long lastPanStepMs = 0;
  unsigned long lastNtpAttemptMs = 0;
  bool ntpAttempted = false;
  bool timeSynced = false;
  float phase = 0.0f;
  uint8_t illuminationPercent = 0;
  uint8_t topRow = 0;
};

State state;

uint8_t maxTopRow() {
  return MOON_CANVAS_HEIGHT > DISPLAY_HEIGHT
             ? static_cast<uint8_t>(MOON_CANVAS_HEIGHT - DISPLAY_HEIGHT)
             : 0;
}

uint8_t sanitizedBrightness(uint8_t brightness) {
  return brightness > 15 ? 15 : brightness;
}

void beginFrame() {
  programRuntimeContext().beginFrame();
  programRuntimeContext().clearFrame();
}

void endFrame() { programRuntimeContext().endFrame(); }

void setPixel(int16_t row, int16_t col) {
  if (row >= 0 && row < DISPLAY_HEIGHT && col >= 0 && col < DISPLAY_WIDTH) {
    programRuntimeContext().setPoint(row, col, true);
  }
}

double julianDay(int year, int month, int day, int hour, int minute,
                 int second) {
  int y = year;
  int m = month;
  if (m <= 2) {
    y--;
    m += 12;
  }
  const int a = y / 100;
  const int b = 2 - a + a / 4;
  const double dayFraction =
      (hour + minute / 60.0 + second / 3600.0) / 24.0;
  return std::floor(365.25 * (y + 4716)) +
         std::floor(30.6001 * (m + 1)) + day + b - 1524.5 + dayFraction;
}

float normalizePhase(float phase) {
  float normalized = phase - std::floor(phase);
  return normalized < 0.0f ? normalized + 1.0f : normalized;
}

float moonPhaseFromJulianDay(double jd) {
  double days = std::fmod(jd - KNOWN_NEW_MOON_JD, SYNODIC_MONTH_DAYS);
  if (days < 0.0) {
    days += SYNODIC_MONTH_DAYS;
  }
  return static_cast<float>(days / SYNODIC_MONTH_DAYS);
}

const char *phaseName(float phase) {
  const int index =
      static_cast<int>(std::floor(normalizePhase(phase) * 8.0f + 0.5f)) % 8;
  static const char *names[] = {
      "New Moon",         "Waxing Crescent", "First Quarter", "Waxing Gibbous",
      "Full Moon",        "Waning Gibbous",  "Last Quarter",  "Waning Crescent"};
  return names[index];
}

uint8_t illuminationPercent(float phase) {
  constexpr float kPi = 3.14159265f;
  const float fraction =
      0.5f * (1.0f - std::cos(2.0f * kPi * normalizePhase(phase)));
  int percent = static_cast<int>(std::lround(fraction * 100.0f));
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  return static_cast<uint8_t>(percent);
}

double daysUntilPhase(float currentPhase, float targetPhase) {
  float delta = normalizePhase(targetPhase) - normalizePhase(currentPhase);
  if (delta <= 0.0001f) {
    delta += 1.0f;
  }
  return static_cast<double>(delta) * SYNODIC_MONTH_DAYS;
}

void appendDuration(char *out, size_t outSize, double days) {
  long totalMinutes = static_cast<long>(
      std::lround((days < 0.0 ? 0.0 : days) * 24.0 * 60.0));
  if (totalMinutes < 1) {
    totalMinutes = 1;
  }
  const long wholeDays = totalMinutes / (24L * 60L);
  const long hours = (totalMinutes % (24L * 60L)) / 60L;
  const long minutes = totalMinutes % 60L;
  char piece[24];
  if (wholeDays > 0) {
    snprintf(piece, sizeof(piece), "%ldd %ldh", wholeDays, hours);
  } else if (hours > 0) {
    snprintf(piece, sizeof(piece), "%ldh %ldm", hours, minutes);
  } else {
    snprintf(piece, sizeof(piece), "%ldm", minutes);
  }
  const size_t used = strlen(out);
  if (used + 1 < outSize) {
    strncat(out, piece, outSize - used - 1);
  }
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
  char *buffer = programRuntimeContext().textBuffer();
  const size_t size = programRuntimeContext().textBufferSize();
  if (!buffer || size == 0) {
    return;
  }
  snprintf(buffer, size, "%s %u%%", phaseName(state.phase),
           static_cast<unsigned>(state.illuminationPercent));
  appendUpcoming(buffer, size, "New", state.phase, 0.0f);
  appendUpcoming(buffer, size, "1st Qtr", state.phase, 0.25f);
  appendUpcoming(buffer, size, "Full", state.phase, 0.5f);
  appendUpcoming(buffer, size, "Last Qtr", state.phase, 0.75f);
}

bool syncTimeIfNeeded(unsigned long now) {
  if (state.timeSynced) {
    return true;
  }

  if (time(nullptr) >= MIN_VALID_TIME) {
    state.timeSynced = true;
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (!state.ntpAttempted ||
      now - state.lastNtpAttemptMs >= NTP_RETRY_MS) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    state.lastNtpAttemptMs = now;
    state.ntpAttempted = true;
  }

  if (time(nullptr) >= MIN_VALID_TIME) {
    state.timeSynced = true;
  }
  return state.timeSynced;
}

void updatePhaseFromClockOrDemo(unsigned long now) {
  if (syncTimeIfNeeded(now)) {
    const time_t nowSecs = time(nullptr);
    struct tm utc;
    gmtime_r(&nowSecs, &utc);
    state.phase = moonPhaseFromJulianDay(
        julianDay(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                  utc.tm_min, utc.tm_sec));
  } else {
    state.phase = std::fmod((now / 1000.0f) / 60.0f, 1.0f);
  }
  state.phase = normalizePhase(state.phase);
  state.illuminationPercent = illuminationPercent(state.phase);
}

void moonUv(int16_t canvasRow, int16_t col, float &u, float &v) {
  constexpr float cx = (DISPLAY_WIDTH - 1) / 2.0f;
  constexpr float cy = (MOON_CANVAS_HEIGHT - 1) / 2.0f;
  constexpr float radius = (MOON_DIAMETER - 1) / 2.0f;
  u = (cx - static_cast<float>(col)) / radius;
  v = (static_cast<float>(canvasRow) - cy) / radius;
}

bool inMoonDisk(int16_t canvasRow, int16_t col) {
  float u;
  float v;
  moonUv(canvasRow, col, u, v);
  return u * u + v * v <= 1.0f;
}

bool moonIlluminated(int16_t canvasRow, int16_t col, float phase) {
  float u;
  float v;
  moonUv(canvasRow, col, u, v);
  if (u * u + v * v > 1.0f) {
    return false;
  }
  constexpr float kPi = 3.14159265f;
  const float normalized = normalizePhase(phase);
  const float limb = std::cos(2.0f * kPi * normalized);
  return normalized <= 0.5f ? u >= limb : u <= -limb;
}

bool onMoonRim(int16_t canvasRow, int16_t col) {
  return inMoonDisk(canvasRow, col) &&
         (!inMoonDisk(canvasRow - 1, col) ||
          !inMoonDisk(canvasRow + 1, col) ||
          !inMoonDisk(canvasRow, col - 1) ||
          !inMoonDisk(canvasRow, col + 1));
}

void renderMoonFrame() {
  beginFrame();
  for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
    const int16_t canvasRow = static_cast<int16_t>(state.topRow) + row;
    for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
      if (moonIlluminated(canvasRow, col, state.phase) ||
          onMoonRim(canvasRow, col)) {
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
}

void startNameScroll(const ProgramConfig &cfg) {
  tinker::RuntimeContext &runtime = programRuntimeContext();
  buildScrollText();
  runtime.clearText();
  runtime.setBrightness(sanitizedBrightness(cfg.brightness));
  char *buffer = runtime.textBuffer();
  runtime.startTextScroll(buffer && buffer[0] ? buffer : phaseName(state.phase),
                          tinker::TextAlignment::Left, cfg.scrollSpeedMs);
}

void enterView(View view, const ProgramConfig &cfg, unsigned long now) {
  state.view = view;
  if (view == View::Moon) {
    resetMoonPan(now);
    renderMoonFrame();
  } else {
    startNameScroll(cfg);
  }
}

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
      return true;
    }
    break;
  }
  return false;
}
} // namespace

void moonPhaseStart(const ProgramConfig &cfg) {
  programRuntimeContext().setBrightness(sanitizedBrightness(cfg.brightness));
  programRuntimeContext().clearText();
  state = State{};
  const unsigned long now = millis();
  updatePhaseFromClockOrDemo(now);
  enterView(View::Moon, cfg, now);
}

void moonPhaseTick(const ProgramConfig &cfg) {
  const unsigned long now = millis();
  if (state.view == View::Name) {
    updatePhaseFromClockOrDemo(now);
    if (programRuntimeContext().animateText()) {
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
