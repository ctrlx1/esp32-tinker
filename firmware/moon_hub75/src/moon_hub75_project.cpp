#include "moon_hub75_project.h"

#include "app_version.h"
#include "font3x5.h"
#include "full_moon.h"
#include "howl_player.h"
#include "moon_sky.h"
#include "setup_html.h"
#include "welcome_screen.h"

#include <WiFi.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace {

constexpr uint8_t kSettingsVersion = 1;
constexpr unsigned long kHowlIntervalMs = 60UL * 60UL * 1000UL;
constexpr int32_t kHowlPercentScale = 10000;
constexpr int32_t kHowlPercentMax = 100 * kHowlPercentScale;
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

bool argBlank(const String &value) {
  String copy = value;
  copy.trim();
  return copy.length() == 0;
}

bool parseDecimal(String value, double minimum, double maximum, double &result) {
  value.trim();
  if (value.length() == 0) {
    return false;
  }
  char *end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0' || !std::isfinite(parsed) ||
      parsed < minimum || parsed > maximum) {
    return false;
  }
  result = parsed;
  return true;
}

bool parseHowlPercent(String value, int32_t &scaled) {
  value.trim();
  if (value.length() == 0 || value.length() > 8) {
    return false;
  }
  int32_t whole = 0;
  int32_t fraction = 0;
  int fractionDigits = 0;
  bool dot = false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char character = value.charAt(i);
    if (character == '.' && !dot) {
      dot = true;
      continue;
    }
    if (character < '0' || character > '9') {
      return false;
    }
    if (!dot) {
      whole = whole * 10 + (character - '0');
      if (whole > 100) {
        return false;
      }
    } else if (fractionDigits == 4) {
      return false;
    } else {
      fraction = fraction * 10 + (character - '0');
      ++fractionDigits;
    }
  }
  while (fractionDigits < 4) {
    fraction *= 10;
    ++fractionDigits;
  }
  if (whole == 100 && fraction != 0) {
    return false;
  }
  scaled = whole * kHowlPercentScale + fraction;
  return scaled >= 0 && scaled <= kHowlPercentMax;
}

void formatHowlPercent(int32_t scaled, char *out, size_t size) {
  snprintf(out, size, "%ld.%04ld", static_cast<long>(scaled / kHowlPercentScale),
           static_cast<long>(scaled % kHowlPercentScale));
}

void formatLocalClock(time_t utc, int tzOffsetMinutes, char *out, size_t size) {
  int64_t second = (static_cast<int64_t>(utc) +
                    static_cast<int64_t>(tzOffsetMinutes) * 60) %
                   86400;
  if (second < 0) {
    second += 86400;
  }
  const int minuteOfDay = static_cast<int>(second / 60);
  const int hour24 = minuteOfDay / 60;
  const int minute = minuteOfDay % 60;
  int hour12 = hour24 % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }
  snprintf(out, size, "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
}

void formatPlaceLine(const MoonView &view, int tzOffsetMinutes, bool hasRise,
                     time_t riseUtc, bool hasSet, time_t setUtc,
                     uint8_t distanceUnit, char *out, size_t size) {
  char rise[24];
  char set[24];
  if (hasRise) {
    char clock[16];
    formatLocalClock(riseUtc, tzOffsetMinutes, clock, sizeof(clock));
    snprintf(rise, sizeof(rise), "Moon Rise %s", clock);
  } else {
    snprintf(rise, sizeof(rise), "No rise");
  }
  if (hasSet) {
    char clock[16];
    formatLocalClock(setUtc, tzOffsetMinutes, clock, sizeof(clock));
    snprintf(set, sizeof(set), "Moon Set %s", clock);
  } else {
    snprintf(set, sizeof(set), "No set");
  }
  double azimuth = std::fmod(view.azimuthDeg, 360.0);
  if (azimuth < 0.0) {
    azimuth += 360.0;
  }
  if (azimuth >= 359.99995) {
    azimuth = 0.0;
  }
  double altitude = view.altitudeDeg;
  if (std::fabs(altitude) < 0.00005) {
    altitude = 0.0;
  }
  const int kilometers = static_cast<int>(std::lround(view.distanceKm));
  const int miles =
      static_cast<int>(std::lround(view.distanceKm / 1.609344));
  char distance[32];
  if (distanceUnit == 1) {
    snprintf(distance, sizeof(distance), "Distance\n%dmi", miles);
  } else if (distanceUnit == 2) {
    snprintf(distance, sizeof(distance), "Distance\n%dkm", kilometers);
  } else {
    snprintf(distance, sizeof(distance), "Distance\n%dmi\n%dkm", miles,
             kilometers);
  }
  snprintf(out, size, "\n%s\n%s\n%s\nMoon Az %.4f\nMoon Alt %.4f", rise, set,
           distance,
           azimuth, altitude);
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

bool moonLit(uint8_t row, uint8_t col, float phase, bool oriented,
             float brightDeg) {
  float u;
  float v;
  moonUv(row, col, u, v);
  constexpr float kPi = 3.14159265f;
  const float normalized = normalizePhase(phase);
  float along = u;
  float span = v;
  if (oriented) {
    const float radians = brightDeg * kPi / 180.0f;
    const float up = -v;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    along = u * sine + up * cosine;
    span = u * cosine - up * sine;
  }
  const float disk = 1.0f - span * span;
  const float halfWidth = std::sqrt(disk > 0.0f ? disk : 0.0f);
  const float limb = std::cos(2.0f * kPi * normalized) * halfWidth;
  if (!oriented) {
    // Waxing light is on the right, matching the northern sky.
    return normalized <= 0.5f ? u >= limb : u <= -limb;
  }
  return along >= limb;
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

double illuminationPercent(float phase) {
  constexpr double kPi = 3.141592653589793;
  const double fraction =
      0.5 * (1.0 - std::cos(2.0 * kPi * normalizePhase(phase)));
  double percent = fraction * 100.0;
  if (percent < 0.0) {
    percent = 0.0;
  } else if (percent > 100.0) {
    percent = 100.0;
  }
  return percent;
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
  strncat(out, "\n", outSize - used - 1);
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
  settings_.textBrightness =
      preferences.getUChar("textBright", settings_.brightness);
  if (settings_.textBrightness > settings_.brightness) {
    settings_.textBrightness = settings_.brightness;
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

  settings_.distanceUnit = preferences.getUChar("distUnit", 0);
  if (settings_.distanceUnit >= kDistanceUnitCount) {
    settings_.distanceUnit = 0;
  }

  settings_.hasLocation = preferences.getUChar("hasPlace", 0) != 0;
  if (settings_.hasLocation) {
    settings_.latitude = preferences.getFloat("lat", 0);
    settings_.longitude = preferences.getFloat("lon", 0);
    const int32_t tzMinutes = preferences.getInt("tzMin", 0);
    if (settings_.latitude < -90.0f || settings_.latitude > 90.0f ||
        settings_.longitude < -180.0f || settings_.longitude > 180.0f ||
        tzMinutes < -12 * 60 || tzMinutes > 14 * 60) {
      settings_.hasLocation = false;
    } else {
      settings_.tzOffsetMinutes = static_cast<int16_t>(tzMinutes);
    }
  }

  settings_.howlBefore = preferences.getInt("howlBefore", 0);
  settings_.howlAfter = preferences.getInt("howlAfter", 0);
  if (settings_.howlBefore < 0 || settings_.howlBefore > kHowlPercentMax) {
    settings_.howlBefore = 0;
  }
  if (settings_.howlAfter < 0 || settings_.howlAfter > kHowlPercentMax) {
    settings_.howlAfter = 0;
  }

  Serial.print(" moon_hub75 brightness=");
  Serial.print(settings_.brightness);
  Serial.print(" text=");
  Serial.print(settings_.textBrightness);
  Serial.print(" scroll=");
  Serial.print(settings_.scrollSpeedMs);
  Serial.print(" color=");
  Serial.print(kScrollColors[settings_.textColor].name);
  Serial.print(" distance=");
  Serial.print(kDistanceUnits[settings_.distanceUnit]);
  char howlBefore[12];
  char howlAfter[12];
  formatHowlPercent(settings_.howlBefore, howlBefore, sizeof(howlBefore));
  formatHowlPercent(settings_.howlAfter, howlAfter, sizeof(howlAfter));
  Serial.print(" howl=");
  Serial.print(howlBefore);
  Serial.print("/");
  Serial.print(howlAfter);
  if (settings_.hasLocation) {
    Serial.print(" place=");
    Serial.print(settings_.latitude, 4);
    Serial.print(",");
    Serial.print(settings_.longitude, 4);
    Serial.print(" tzMin=");
    Serial.println(settings_.tzOffsetMinutes);
  } else {
    Serial.println();
  }
}

void MoonHub75Project::saveSettings(Preferences &preferences) const {
  preferences.putUChar("brightness", settings_.brightness);
  preferences.putUChar("textBright", settings_.textBrightness);
  preferences.putUInt("scrollSpeed", settings_.scrollSpeedMs);
  preferences.putUChar("textColor", settings_.textColor);
  preferences.putUChar("distUnit", settings_.distanceUnit);
  preferences.putUChar("hasPlace", settings_.hasLocation ? 1 : 0);
  preferences.putInt("howlBefore", settings_.howlBefore);
  preferences.putInt("howlAfter", settings_.howlAfter);
  if (settings_.hasLocation) {
    preferences.putFloat("lat", settings_.latitude);
    preferences.putFloat("lon", settings_.longitude);
    preferences.putInt("tzMin", settings_.tzOffsetMinutes);
  }
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
  page.replace("TEXT_LEVEL_PLACEHOLDER", String(settings_.textBrightness));
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
  String distanceOptions;
  for (uint8_t i = 0; i < kDistanceUnitCount; ++i) {
    distanceOptions += "<option value=\"";
    distanceOptions += String(i);
    distanceOptions += "\"";
    if (i == settings_.distanceUnit) {
      distanceOptions += " selected";
    }
    distanceOptions += ">";
    distanceOptions += kDistanceUnits[i];
    distanceOptions += "</option>";
  }
  page.replace("DISTANCE_OPTIONS_PLACEHOLDER", distanceOptions);
  char latitude[24] = "";
  char longitude[24] = "";
  char timezone[24] = "";
  if (settings_.hasLocation) {
    snprintf(latitude, sizeof(latitude), "%.5f", settings_.latitude);
    snprintf(longitude, sizeof(longitude), "%.5f", settings_.longitude);
    if (settings_.tzOffsetMinutes % 60 == 0) {
      snprintf(timezone, sizeof(timezone), "%d",
               settings_.tzOffsetMinutes / 60);
    } else {
      snprintf(timezone, sizeof(timezone), "%.2f",
               settings_.tzOffsetMinutes / 60.0);
    }
  }
  page.replace("LAT_PLACEHOLDER", latitude);
  page.replace("LON_PLACEHOLDER", longitude);
  page.replace("TZ_PLACEHOLDER", timezone);
  char howlBefore[12];
  char howlAfter[12];
  formatHowlPercent(settings_.howlBefore, howlBefore, sizeof(howlBefore));
  formatHowlPercent(settings_.howlAfter, howlAfter, sizeof(howlAfter));
  page.replace("HOWL_BEFORE_PLACEHOLDER", howlBefore);
  page.replace("HOWL_AFTER_PLACEHOLDER", howlAfter);
  return page;
}

bool MoonHub75Project::applyPortalRequest(
    const tinker::PortalRequest &request, String &error) {
  if (!request.hasArg("speed") || !request.hasArg("brightness") ||
      !request.hasArg("textBright") || !request.hasArg("color") ||
      !request.hasArg("distance") || !request.hasArg("howlBefore") ||
      !request.hasArg("howlAfter")) {
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

  uint32_t textBrightness;
  if (!parseUnsignedInRange(request.arg("textBright"), 0, brightness,
                            textBrightness)) {
    error = "Text brightness must be an integer from 0 to the panel brightness.";
    return false;
  }

  uint32_t color;
  if (!parseUnsignedInRange(request.arg("color"), 0, kScrollColorCount - 1,
                            color)) {
    error = "Choose a scroll color from the list.";
    return false;
  }

  uint32_t distance;
  if (!parseUnsignedInRange(request.arg("distance"), 0, kDistanceUnitCount - 1,
                            distance)) {
    error = "Choose a moon distance unit from the list.";
    return false;
  }

  bool hasLocation = settings_.hasLocation;
  float latitude = settings_.latitude;
  float longitude = settings_.longitude;
  int16_t tzOffsetMinutes = settings_.tzOffsetMinutes;
  const bool latitudeBlank =
      !request.hasArg("lat") || argBlank(request.arg("lat"));
  const bool longitudeBlank =
      !request.hasArg("lon") || argBlank(request.arg("lon"));
  const bool timezoneBlank = !request.hasArg("tz") || argBlank(request.arg("tz"));
  if (!latitudeBlank || !longitudeBlank || !timezoneBlank) {
    if (latitudeBlank || longitudeBlank || timezoneBlank) {
      error = "Enter latitude, longitude, and timezone together.";
      return false;
    }
    double latitudeValue = 0;
    double longitudeValue = 0;
    double timezoneHours = 0;
    if (!parseDecimal(request.arg("lat"), -90.0, 90.0, latitudeValue)) {
      error = "Latitude must be a number from -90 to 90.";
      return false;
    }
    if (!parseDecimal(request.arg("lon"), -180.0, 180.0, longitudeValue)) {
      error = "Longitude must be a number from -180 to 180.";
      return false;
    }
    if (!parseDecimal(request.arg("tz"), -12.0, 14.0, timezoneHours)) {
      error = "Timezone must be hours from -12 to 14.";
      return false;
    }
    const long minutes = std::lround(timezoneHours * 60.0);
    if (minutes < -12 * 60 || minutes > 14 * 60) {
      error = "Timezone must be hours from -12 to 14.";
      return false;
    }
    hasLocation = true;
    latitude = static_cast<float>(latitudeValue);
    longitude = static_cast<float>(longitudeValue);
    tzOffsetMinutes = static_cast<int16_t>(minutes);
  }

  int32_t howlBefore = 0;
  int32_t howlAfter = 0;
  if (!parseHowlPercent(request.arg("howlBefore"), howlBefore)) {
    error = "Howl start must be a percent from 0.0000 to 100.0000.";
    return false;
  }
  if (!parseHowlPercent(request.arg("howlAfter"), howlAfter)) {
    error = "Howl end must be a percent from 0.0000 to 100.0000.";
    return false;
  }

  settings_.scrollSpeedMs = static_cast<uint16_t>(speed);
  settings_.brightness = static_cast<uint8_t>(brightness);
  settings_.textBrightness = static_cast<uint8_t>(textBrightness);
  settings_.textColor = static_cast<uint8_t>(color);
  settings_.distanceUnit = static_cast<uint8_t>(distance);
  settings_.hasLocation = hasLocation;
  settings_.latitude = latitude;
  settings_.longitude = longitude;
  settings_.tzOffsetMinutes = tzOffsetMinutes;
  settings_.howlBefore = howlBefore;
  settings_.howlAfter = howlAfter;
  return true;
}

bool MoonHub75Project::howlWindowActive() const {
  const int32_t span = phase_ < 0.5f ? settings_.howlBefore : settings_.howlAfter;
  return illuminationPercent_ + (static_cast<double>(span) / kHowlPercentScale) >=
         100.0;
}

void MoonHub75Project::maybePlayHowl(unsigned long now) {
  if (!howlWindowActive()) {
    howlWindowArmed_ = true;
    return;
  }
  if (howlPlaying()) {
    return;
  }
  if (!howlWindowArmed_ && lastHowlMs_ != 0 &&
      now - lastHowlMs_ < kHowlIntervalMs) {
    return;
  }
  howlPlay();
  lastHowlMs_ = now;
  howlWindowArmed_ = false;
}

void MoonHub75Project::refreshPlaceText() {
  if (!timeSynced_ || !settings_.hasLocation || time(nullptr) < kMinValidTime) {
    placeText_[0] = '\0';
    placeDate_ = 0;
    placeMinute_ = -1;
    limbOriented_ = false;
    limbAngleDeg_ = 90.0f;
    return;
  }

  const time_t utc = time(nullptr);
  const time_t localClock = static_cast<time_t>(
      static_cast<int64_t>(utc) +
      static_cast<int64_t>(settings_.tzOffsetMinutes) * 60);
  struct tm localTime;
  if (gmtime_r(&localClock, &localTime) == nullptr) {
    placeText_[0] = '\0';
    limbOriented_ = false;
    limbAngleDeg_ = 90.0f;
    return;
  }
  const int date = (localTime.tm_year + 1900) * 10000 +
                   (localTime.tm_mon + 1) * 100 + localTime.tm_mday;
  const bool moved = date != placeDate_ || settings_.latitude != placeLat_ ||
                     settings_.longitude != placeLon_ ||
                     settings_.tzOffsetMinutes != placeTz_;
  const int64_t minuteKey = static_cast<int64_t>(utc) / 60;
  if (moved) {
    const MoonView day =
        moonViewForDay(utc, settings_.tzOffsetMinutes, settings_.latitude,
                       settings_.longitude);
    placeHasRise_ = day.hasRise;
    placeHasSet_ = day.hasSet;
    placeRiseUtc_ = day.riseUtc;
    placeSetUtc_ = day.setUtc;
    placeDate_ = date;
    placeLat_ = settings_.latitude;
    placeLon_ = settings_.longitude;
    placeTz_ = settings_.tzOffsetMinutes;
    formatPlaceLine(day, settings_.tzOffsetMinutes, placeHasRise_,
                    placeRiseUtc_, placeHasSet_, placeSetUtc_,
                    settings_.distanceUnit, placeText_, sizeof(placeText_));
    limbAngleDeg_ = static_cast<float>(moonBrightLimbDeg(
        utc, settings_.latitude, settings_.longitude));
    limbOriented_ = true;
    placeMinute_ = minuteKey;
    return;
  }
  if (minuteKey == placeMinute_) {
    return;
  }
  const MoonView now =
      moonViewAt(utc, settings_.latitude, settings_.longitude);
  formatPlaceLine(now, settings_.tzOffsetMinutes, placeHasRise_, placeRiseUtc_,
                  placeHasSet_, placeSetUtc_, settings_.distanceUnit, placeText_,
                  sizeof(placeText_));
  limbAngleDeg_ = static_cast<float>(
      moonBrightLimbDeg(utc, settings_.latitude, settings_.longitude));
  limbOriented_ = true;
  placeMinute_ = minuteKey;
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
  refreshPlaceText();
  char message[512];
  snprintf(message, sizeof(message), "%s %.4f%%", phaseName(phase_),
           illuminationPercent_);
  appendUpcoming(message, sizeof(message), "New", phase_, 0.0f);
  appendUpcoming(message, sizeof(message), "1st Qtr", phase_, 0.25f);
  appendUpcoming(message, sizeof(message), "Full", phase_, 0.5f);
  appendUpcoming(message, sizeof(message), "Last Qtr", phase_, 0.75f);
  if (placeText_[0] != '\0') {
    const size_t used = strlen(message);
    if (used + 1 < sizeof(message)) {
      strncat(message, placeText_, sizeof(message) - used - 1);
    }
  }

  char wrapped[kMaxLines][kMaxLineChars + 1] = {};
  uint8_t count = 0;
  char current[kMaxLineChars + 1] = "";
  const char *cursor = message;
  while (*cursor && count < kMaxLines) {
    if (*cursor == '\n') {
      if (current[0] != '\0') {
        strncpy(wrapped[count], current, kMaxLineChars);
        wrapped[count][kMaxLineChars] = '\0';
        ++count;
        current[0] = '\0';
      }
      ++cursor;
      continue;
    }
    while (*cursor == ' ') {
      ++cursor;
    }
    if (!*cursor) {
      break;
    }
    if (*cursor == '\n') {
      continue;
    }
    char word[32];
    size_t length = 0;
    while (*cursor && *cursor != ' ' && *cursor != '\n' &&
           length + 1 < sizeof(word)) {
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
      if (!moonInDisk(row, col) ||
          !moonLit(row, col, phase_, limbOriented_, limbAngleDeg_)) {
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
  uint8_t textRed = color.red;
  uint8_t textGreen = color.green;
  uint8_t textBlue = color.blue;
  if (settings_.brightness == 0 || settings_.textBrightness == 0) {
    textRed = 0;
    textGreen = 0;
    textBlue = 0;
  } else if (settings_.textBrightness < settings_.brightness) {
    textRed = scaleChannel(textRed, settings_.textBrightness, settings_.brightness);
    textGreen =
        scaleChannel(textGreen, settings_.textBrightness, settings_.brightness);
    textBlue =
        scaleChannel(textBlue, settings_.textBrightness, settings_.brightness);
  }
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
                                 static_cast<uint16_t>(px), textRed, textGreen,
                                 textBlue);
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
  placeDate_ = 0;
  placeMinute_ = -1;
  placeText_[0] = '\0';
  lineCount_ = 0;
  contentHeight_ = 0;
  runtime_.setBrightness(settings_.brightness);
  if (!welcomePlayed_ && WiFi.status() == WL_CONNECTED) {
    welcomePlayed_ = true;
    runWelcome(runtime_);
    lastScrollMs_ = millis();
  }
  updatePhase(lastScrollMs_);
  if (howlWindowActive()) {
    lastHowlMs_ = lastScrollMs_;
    howlWindowArmed_ = false;
  }
  rebuildLines();
  drawFrame();
}

void MoonHub75Project::tickPrograms() {
  if (!started_) {
    startPrograms();
    return;
  }

  const unsigned long now = millis();
  maybePlayHowl(now);
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
