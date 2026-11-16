#include "moon_hub75_project.h"

#include "app_version.h"
#include "font3x5.h"
#include "full_moon.h"
#include "setup_html.h"
#include "welcome_screen.h"

#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr uint8_t kDefaultBrightness = 4;
constexpr uint16_t kDefaultScrollSpeedMs = 75;
constexpr uint8_t kPanelHeight = 32;
constexpr uint8_t kTextWidth = 31;
constexpr uint8_t kMoonColumn = 32;
constexpr uint8_t kLinePitch = 6;
constexpr uint8_t kMaxLineChars = 8;
constexpr uint8_t kMaxLines = 48;
constexpr unsigned long kNtpRetryMs = 30000UL;
constexpr time_t kMinValidTime = 1609459200;
constexpr double kSynodicMonthDays = 29.530588853;
constexpr double kKnownNewMoonJd = 2451550.1;

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

bool parseUnsignedInRange(String value, uint32_t minimum, uint32_t maximum,
                          uint32_t &result) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }

  uint32_t parsed = 0;
  for (size_t i = 0; i < value.length(); i++) {
    const char digit = value.charAt(i);
    if (digit < '0' || digit > '9') {
      return false;
    }
    const uint32_t next = parsed * 10U + static_cast<uint32_t>(digit - '0');
    if (next < parsed || next > maximum) {
      return false;
    }
    parsed = next;
  }

  if (parsed < minimum) {
    return false;
  }
  result = parsed;
  return true;
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
  double days = std::fmod(jd - kKnownNewMoonJd, kSynodicMonthDays);
  if (days < 0.0) {
    days += kSynodicMonthDays;
  }
  return static_cast<float>(days / kSynodicMonthDays);
}

const char *phaseName(float phase) {
  const int index =
      static_cast<int>(std::floor(normalizePhase(phase) * 8.0f + 0.5f)) % 8;
  static const char *names[] = {
      "New Moon",         "Waxing Crescent", "First Quarter", "Waxing Gibbous",
      "Full Moon",        "Waning Gibbous",  "Last Quarter",  "Waning Crescent"};
  return names[index];
}

void moonUv(uint8_t row, uint8_t col, float &u, float &v) {
  constexpr float center = (kFullMoonSize - 1) / 2.0f;
  constexpr float radius = center;
  u = (static_cast<float>(col) - center) / radius;
  v = (static_cast<float>(row) - center) / radius;
}

bool moonInDisk(int row, int col) {
  if (row < 0 || col < 0 || row >= kFullMoonSize || col >= kFullMoonSize) {
    return false;
  }
  float u;
  float v;
  moonUv(static_cast<uint8_t>(row), static_cast<uint8_t>(col), u, v);
  return u * u + v * v <= 1.0f;
}

bool moonLit(uint8_t row, uint8_t col, float phase) {
  float u;
  float v;
  moonUv(row, col, u, v);
  const float disk = 1.0f - v * v;
  const float halfWidth = std::sqrt(disk > 0.0f ? disk : 0.0f);
  constexpr float kPi = 3.14159265f;
  const float normalized = normalizePhase(phase);
  const float limb = std::cos(2.0f * kPi * normalized) * halfWidth;
  // Waxing light is on the right, matching the northern sky.
  return normalized <= 0.5f ? u >= limb : u <= -limb;
}

bool moonOnRim(uint8_t row, uint8_t col) {
  const int y = row;
  const int x = col;
  return moonInDisk(y, x) &&
         (!moonInDisk(y - 1, x) || !moonInDisk(y + 1, x) ||
          !moonInDisk(y, x - 1) || !moonInDisk(y, x + 1));
}

uint8_t scaleChannel(uint8_t value, uint8_t numerator, uint8_t denominator) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * numerator) /
                              denominator);
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
  return static_cast<double>(delta) * kSynodicMonthDays;
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

} // namespace

MoonHub75Project::MoonHub75Project(tinker::RuntimeContext &runtime)
    : runtime_(runtime) {}

tinker::ProjectDefinition MoonHub75Project::definition() const {
  return {"moon_hub75",
          "Moon HUB75",
          APP_VERSION,
          "Moon-HUB75-",
          {"moon_hub75", "cfgVer", kSettingsVersion}};
}

bool MoonHub75Project::migrateSettings(Preferences &, uint16_t storedVersion) {
  return storedVersion <= kSettingsVersion;
}

void MoonHub75Project::loadSettings(Preferences &preferences) {
  settings_.brightness =
      preferences.getUChar("brightness", kDefaultBrightness);
  if (settings_.brightness > 15) {
    settings_.brightness = kDefaultBrightness;
  }

  const uint32_t storedScrollSpeed =
      preferences.getUInt("scrollSpeed", kDefaultScrollSpeedMs);
  settings_.scrollSpeedMs =
      storedScrollSpeed >= MOON_HUB75_MIN_SCROLL_SPEED_MS &&
              storedScrollSpeed <= MOON_HUB75_MAX_SCROLL_SPEED_MS
          ? static_cast<uint16_t>(storedScrollSpeed)
          : kDefaultScrollSpeedMs;

  settings_.textColor = preferences.getUChar("textColor", 0);
  if (settings_.textColor >= kScrollColorCount) {
    settings_.textColor = 0;
  }

  Serial.print(" moon_hub75 brightness=");
  Serial.print(settings_.brightness);
  Serial.print(" scroll=");
  Serial.print(settings_.scrollSpeedMs);
  Serial.print(" color=");
  Serial.println(kScrollColors[settings_.textColor].name);
}

void MoonHub75Project::saveSettings(Preferences &preferences) const {
  preferences.putUChar("brightness", settings_.brightness);
  preferences.putUInt("scrollSpeed", settings_.scrollSpeedMs);
  preferences.putUChar("textColor", settings_.textColor);
}

uint8_t MoonHub75Project::displayBrightness() const {
  return settings_.brightness;
}

String MoonHub75Project::buildPortalPage(const String &ip,
                                         const String &ssid) const {
  String page = FPSTR(SETUP_PORTAL_HTML);
  page.replace("IP_PLACEHOLDER", htmlEscape(ip));
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("SPEED_PLACEHOLDER", String(settings_.scrollSpeedMs));
  page.replace("BRIGHTNESS_PLACEHOLDER", String(settings_.brightness));
  String options;
  for (uint8_t i = 0; i < kScrollColorCount; ++i) {
    options += "<option value=\"";
    options += String(i);
    options += "\"";
    if (i == settings_.textColor) {
      options += " selected";
    }
    options += ">";
    options += kScrollColors[i].name;
    options += "</option>";
  }
  page.replace("COLOR_OPTIONS_PLACEHOLDER", options);
  return page;
}

bool MoonHub75Project::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("speed") || !request.hasArg("brightness") ||
      !request.hasArg("color")) {
    error = "Missing moon_hub75 settings.";
    return false;
  }

  uint32_t speed;
  if (!parseUnsignedInRange(request.arg("speed"),
                            MOON_HUB75_MIN_SCROLL_SPEED_MS,
                            MOON_HUB75_MAX_SCROLL_SPEED_MS, speed)) {
    error = "Scroll speed must be an integer from 1 to 10000 ms.";
    return false;
  }

  uint32_t brightness;
  if (!parseUnsignedInRange(request.arg("brightness"), 0, 15, brightness)) {
    error = "Brightness must be an integer from 0 to 15.";
    return false;
  }

  uint32_t color;
  if (!parseUnsignedInRange(request.arg("color"), 0, kScrollColorCount - 1,
                            color)) {
    error = "Choose a scroll color from the list.";
    return false;
  }

  settings_.scrollSpeedMs = static_cast<uint16_t>(speed);
  settings_.brightness = static_cast<uint8_t>(brightness);
  settings_.textColor = static_cast<uint8_t>(color);
  return true;
}

void MoonHub75Project::updatePhase(unsigned long now) {
  bool synced = timeSynced_;
  if (!synced && time(nullptr) >= kMinValidTime) {
    timeSynced_ = true;
    synced = true;
  }
  if (!synced && WiFi.status() == WL_CONNECTED &&
      (!ntpAttempted_ || now - lastNtpAttemptMs_ >= kNtpRetryMs)) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lastNtpAttemptMs_ = now;
    ntpAttempted_ = true;
    if (time(nullptr) >= kMinValidTime) {
      timeSynced_ = true;
      synced = true;
    }
  }

  if (synced || timeSynced_) {
    const time_t nowSecs = time(nullptr);
    struct tm utc;
    gmtime_r(&nowSecs, &utc);
    phase_ = moonPhaseFromJulianDay(
        julianDay(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                  utc.tm_min, utc.tm_sec));
  } else {
    phase_ = std::fmod((now / 1000.0f) / 60.0f, 1.0f);
  }
  phase_ = normalizePhase(phase_);
  illuminationPercent_ = illuminationPercent(phase_);
}

void MoonHub75Project::rebuildLines() {
  char message[384];
  snprintf(message, sizeof(message), "%s %u%%", phaseName(phase_),
           static_cast<unsigned>(illuminationPercent_));
  appendUpcoming(message, sizeof(message), "New", phase_, 0.0f);
  appendUpcoming(message, sizeof(message), "1st Qtr", phase_, 0.25f);
  appendUpcoming(message, sizeof(message), "Full", phase_, 0.5f);
  appendUpcoming(message, sizeof(message), "Last Qtr", phase_, 0.75f);

  char wrapped[kMaxLines][kMaxLineChars + 1] = {};
  uint8_t count = 0;
  char current[kMaxLineChars + 1] = "";
  const char *cursor = message;
  while (*cursor && count < kMaxLines) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (!*cursor) {
      break;
    }
    char word[32];
    size_t length = 0;
    while (*cursor && *cursor != ' ' && length + 1 < sizeof(word)) {
      word[length++] = *cursor++;
    }
    word[length] = '\0';
    size_t used = 0;
    while (used < length && count < kMaxLines) {
      const size_t remaining = length - used;
      const size_t piece = remaining > kMaxLineChars ? kMaxLineChars : remaining;
      const bool fits =
          current[0] == '\0' || strlen(current) + 1 + piece <= kMaxLineChars;
      if (!fits) {
        strncpy(wrapped[count], current, kMaxLineChars);
        wrapped[count][kMaxLineChars] = '\0';
        ++count;
        current[0] = '\0';
        continue;
      }
      if (current[0] != '\0') {
        strncat(current, " ", sizeof(current) - strlen(current) - 1);
      }
      strncat(current, word + used, piece);
      used += piece;
      if (used < length) {
        strncpy(wrapped[count], current, kMaxLineChars);
        wrapped[count][kMaxLineChars] = '\0';
        ++count;
        current[0] = '\0';
      }
    }
  }
  if (current[0] != '\0' && count < kMaxLines) {
    strncpy(wrapped[count], current, kMaxLineChars);
    wrapped[count][kMaxLineChars] = '\0';
    ++count;
  }
  if (count == 0) {
    strncpy(wrapped[0], "Moon", kMaxLineChars);
    count = 1;
  }

  bool changed = count != lineCount_;
  for (uint8_t i = 0; !changed && i < count; ++i) {
    changed = strcmp(lines_[i], wrapped[i]) != 0;
  }
  if (!changed) {
    return;
  }
  lineCount_ = count;
  memcpy(lines_, wrapped, sizeof(lines_));
  contentHeight_ = static_cast<int>(lineCount_) * kLinePitch;
  const int cycle = kPanelHeight + contentHeight_;
  if (scrollOffset_ >= cycle) {
    scrollOffset_ = kPanelHeight;
  }
}

void MoonHub75Project::drawFrame() {
  if (!runtime_.hasColor()) {
    Serial.println("Moon HUB75 display is missing color.");
    started_ = false;
    return;
  }

  runtime_.beginColorFrame();
  runtime_.fillColor(0, 0, 0);
  for (uint8_t row = 0; row < kFullMoonSize; ++row) {
    for (uint8_t col = 0; col < kFullMoonSize; ++col) {
      const uint16_t pixel =
          pgm_read_word(&kFullMoon[static_cast<size_t>(row) * kFullMoonSize + col]);
      if (pixel == 0) {
        continue;
      }
      uint8_t red = static_cast<uint8_t>(((pixel >> 11) & 0x1F) << 3);
      uint8_t green = static_cast<uint8_t>(((pixel >> 5) & 0x3F) << 2);
      uint8_t blue = static_cast<uint8_t>((pixel & 0x1F) << 3);
      if (!moonInDisk(row, col) || !moonLit(row, col, phase_)) {
        const bool rim = moonOnRim(row, col);
        red = scaleChannel(red, 1, rim ? 2 : 6);
        green = scaleChannel(green, 1, rim ? 2 : 6);
        blue = scaleChannel(blue, 1, rim ? 2 : 6);
      }
      runtime_.setPixelColor(row, static_cast<uint16_t>(kMoonColumn + col), red,
                             green, blue);
    }
  }

  const ScrollColor &color = kScrollColors[settings_.textColor];
  for (uint8_t line = 0; line < lineCount_; ++line) {
    const int top =
        kPanelHeight + static_cast<int>(line) * kLinePitch - scrollOffset_;
    if (top >= kPanelHeight || top + font3x5::kHeight <= 0) {
      continue;
    }
    int x = 0;
    for (const char *glyph = lines_[line]; *glyph != '\0'; ++glyph) {
      if (x > 0) {
        ++x;
      }
      const uint8_t *columns = font3x5::glyph(*glyph);
      for (uint8_t col = 0; col < font3x5::kWidth; ++col) {
        const int px = x + col;
        if (px < 0 || px >= kTextWidth) {
          continue;
        }
        const uint8_t bits = columns[col];
        for (uint8_t row = 0; row < font3x5::kHeight; ++row) {
          if ((bits & (1U << row)) == 0) {
            continue;
          }
          const int py = top + row;
          if (py < 0 || py >= kPanelHeight) {
            continue;
          }
          runtime_.setPixelColor(static_cast<uint8_t>(py),
                                 static_cast<uint16_t>(px), color.red,
                                 color.green, color.blue);
        }
      }
      x += font3x5::kWidth;
    }
  }
  runtime_.endColorFrame();
  started_ = true;
}

void MoonHub75Project::startPrograms() {
  timeSynced_ = false;
  ntpAttempted_ = false;
  lastNtpAttemptMs_ = 0;
  lastScrollMs_ = millis();
  scrollOffset_ = kPanelHeight;
  lineCount_ = 0;
  contentHeight_ = 0;
  runtime_.setBrightness(settings_.brightness);
  if (!welcomePlayed_ && WiFi.status() == WL_CONNECTED) {
    welcomePlayed_ = true;
    runWelcome(runtime_);
    lastScrollMs_ = millis();
  }
  updatePhase(lastScrollMs_);
  rebuildLines();
  drawFrame();
}

void MoonHub75Project::tickPrograms() {
  if (!started_) {
    startPrograms();
    return;
  }

  const unsigned long now = millis();
  if (now - lastScrollMs_ < settings_.scrollSpeedMs) {
    return;
  }
  lastScrollMs_ = now;
  updatePhase(now);
  rebuildLines();
  const int cycle = kPanelHeight + contentHeight_;
  if (cycle > 0) {
    scrollOffset_ = (scrollOffset_ + 1) % cycle;
  }
  drawFrame();
}
