#include "justin_project.h"

#include "app_version.h"
#include "programs/fireworks/fireworks.h"
#include "programs/flight_watch.h"
#include "programs/maze_hero/maze_hero.h"
#include "programs/moon_phase.h"
#include "programs/pixel_art/pixel_art.h"
#include "programs/real_weather.h"
#include "programs/scroller/scroller.h"
#include "programs/transitions.h"
#include "programs/weather_watch.h"
#include "setup_html.h"

namespace {

constexpr unsigned int DEFAULT_SCROLL_SPEED_MS = 75;
constexpr unsigned int DEFAULT_FIREWORKS_MIN_LAUNCH_DELAY_MS = 60;
constexpr unsigned int DEFAULT_FIREWORKS_MAX_LAUNCH_DELAY_MS = 2000;
constexpr unsigned int DEFAULT_FIREWORKS_ANIM_SPEED_MS = 60;
constexpr unsigned int MIN_MAZE_DIMENSION = 4;
constexpr unsigned int MAX_MAZE_WIDTH = 80;
constexpr unsigned int MAX_MAZE_HEIGHT = 48;
constexpr unsigned int DEFAULT_MAZE_MIN_WIDTH = 8;
constexpr unsigned int DEFAULT_MAZE_MAX_WIDTH = 40;
constexpr unsigned int DEFAULT_MAZE_MIN_HEIGHT = 4;
constexpr unsigned int DEFAULT_MAZE_MAX_HEIGHT = 24;
constexpr unsigned int DEFAULT_MAZE_HERO_MIN_SPEED_MS = 120;
constexpr unsigned int DEFAULT_MAZE_HERO_MAX_SPEED_MS = 200;
constexpr uint8_t DEFAULT_DISPLAY_BRIGHTNESS = 0;
constexpr uint8_t DEFAULT_FIREWORKS_MAX_BRIGHTNESS = 15;

constexpr uint8_t PROGRAM_COUNT = 8;

template <void (*Callback)(const ProgramConfig &)>
void dispatchProgram(void *context, uint8_t) {
  Callback(static_cast<JustinProject *>(context)->settings());
}

uint8_t sanitizeSelectedPrograms(uint8_t selectedPrograms,
                                 ProgramId fallbackProgram) {
  selectedPrograms &= PROGRAM_ALL_FLAGS;
  if (selectedPrograms == 0) {
    selectedPrograms = programIdToFlag(fallbackProgram);
  }
  return selectedPrograms;
}

uint8_t parseSelectedProgramsArg(const String &value) {
  uint8_t selectedPrograms = 0;
  int start = 0;
  while (start < value.length()) {
    int comma = value.indexOf(',', start);
    if (comma < 0) {
      comma = value.length();
    }
    String token = value.substring(start, comma);
    token.trim();
    if (token == "scroller") {
      selectedPrograms |= PROGRAM_SCROLLER_FLAG;
    } else if (token == "fireworks") {
      selectedPrograms |= PROGRAM_FIREWORKS_FLAG;
    } else if (token == "maze_hero") {
      selectedPrograms |= PROGRAM_MAZE_HERO_FLAG;
    } else if (token == "pixel_art") {
      selectedPrograms |= PROGRAM_PIXEL_ART_FLAG;
    } else if (token == "weather_watch") {
      selectedPrograms |= PROGRAM_WEATHER_WATCH_FLAG;
    } else if (token == "real_weather") {
      selectedPrograms |= PROGRAM_REAL_WEATHER_FLAG;
    } else if (token == "moon_phase") {
      selectedPrograms |= PROGRAM_MOON_PHASE_FLAG;
    } else if (token == "flight_watch") {
      selectedPrograms |= PROGRAM_FLIGHT_WATCH_FLAG;
    }
    start = comma + 1;
  }
  return selectedPrograms;
}

ProgramId firstSelectedProgram(uint8_t selectedPrograms,
                               ProgramId fallbackProgram) {
  selectedPrograms =
      sanitizeSelectedPrograms(selectedPrograms, fallbackProgram);
  for (uint8_t index = 0; index < PROGRAM_COUNT; index++) {
    if (selectedPrograms & (uint8_t(1) << index)) {
      return static_cast<ProgramId>(index);
    }
  }
  return fallbackProgram;
}

unsigned long programDurationMs(const ProgramConfig &config) {
  float minutes = config.programDurationMinutes;
  if (minutes <= 0.0f) {
    minutes = DEFAULT_PROGRAM_DURATION_MINUTES;
  }
  if (minutes > MAX_PROGRAM_DURATION_MINUTES) {
    minutes = MAX_PROGRAM_DURATION_MINUTES;
  }
  double durationMs = static_cast<double>(minutes) * 60.0 * 1000.0;
  if (durationMs < 1.0) {
    return 1UL;
  }
  return static_cast<unsigned long>(durationMs);
}

String selectedProgramsToString(uint8_t selectedPrograms) {
  selectedPrograms =
      sanitizeSelectedPrograms(selectedPrograms, ProgramId::Scroller);
  String value;
  for (uint8_t index = 0; index < PROGRAM_COUNT; index++) {
    if (!(selectedPrograms & (uint8_t(1) << index))) {
      continue;
    }
    if (value.length() > 0) {
      value += ",";
    }
    value += programIdToString(static_cast<ProgramId>(index));
  }
  return value;
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

} // namespace

static_assert(static_cast<uint8_t>(ProgramId::Scroller) == 0,
              "Program order must remain persistence-compatible");
static_assert(static_cast<uint8_t>(ProgramId::FlightWatch) == 7,
              "Program order must remain persistence-compatible");

const tinker::ProgramDescriptor JustinProject::kPrograms[8] = {
    {"scroller", &dispatchProgram<scrollerStart>,
     &dispatchProgram<scrollerTick>},
    {"fireworks", &dispatchProgram<fireworksStart>,
     &dispatchProgram<fireworksTick>},
    {"maze_hero", &dispatchProgram<mazeHeroStart>,
     &dispatchProgram<mazeHeroTick>},
    {"pixel_art", &dispatchProgram<pixelArtStart>,
     &dispatchProgram<pixelArtTick>},
    {"weather_watch", &dispatchProgram<weatherWatchStart>,
     &dispatchProgram<weatherWatchTick>},
    {"real_weather", &dispatchProgram<realWeatherStart>,
     &dispatchProgram<realWeatherTick>},
    {"moon_phase", &dispatchProgram<moonPhaseStart>,
     &dispatchProgram<moonPhaseTick>},
    {"flight_watch", &dispatchProgram<flightWatchStart>,
     &dispatchProgram<flightWatchTick>},
};

JustinProject::JustinProject(tinker::RuntimeContext &runtime) {
  setProgramRuntimeContext(runtime);
}

tinker::ProjectDefinition JustinProject::definition() const {
  // A null version key preserves the legacy unversioned NVS layout exactly.
  return {"justin",
          "ESP32 Tinker",
          APP_VERSION,
          "ESP32-Tinker-Setup-",
          {"esp32tinker", nullptr, 0}};
}

bool JustinProject::migrateSettings(Preferences &, uint16_t) {
  // The existing schema is intentionally unchanged. New projects can opt into
  // versioned migrations through the project definition.
  return true;
}

void JustinProject::loadSettings(Preferences &preferences) {
  config_.program =
      parseProgramId(preferences.getString("program", "scroller"));
  uint8_t savedPrograms = preferences.getUChar("programs", 0);
  config_.selectedPrograms =
      savedPrograms == 0
          ? DEFAULT_SELECTED_PROGRAMS
          : sanitizeSelectedPrograms(savedPrograms, config_.program);
  config_.program =
      firstSelectedProgram(config_.selectedPrograms, config_.program);
  config_.programDurationMinutes =
      preferences.getFloat("progDurMin", DEFAULT_PROGRAM_DURATION_MINUTES);
  if (config_.programDurationMinutes <= 0.0f ||
      config_.programDurationMinutes > MAX_PROGRAM_DURATION_MINUTES) {
    config_.programDurationMinutes = DEFAULT_PROGRAM_DURATION_MINUTES;
  }

  config_.scrollMessage = preferences.getString("scrollMsg", "");
  config_.scrollSpeedMs = preferences.getUInt("scrollSpeed", 0);
  if (config_.scrollSpeedMs == 0) {
    config_.scrollSpeedMs = DEFAULT_SCROLL_SPEED_MS;
  }

  config_.fireworksMinLaunchDelayMs =
      preferences.getUInt("fwMinDelayMs", 0);
  if (config_.fireworksMinLaunchDelayMs == 0) {
    config_.fireworksMinLaunchDelayMs =
        DEFAULT_FIREWORKS_MIN_LAUNCH_DELAY_MS;
  }
  config_.fireworksMaxLaunchDelayMs =
      preferences.getUInt("fwMaxDelayMs", 0);
  if (config_.fireworksMaxLaunchDelayMs == 0) {
    config_.fireworksMaxLaunchDelayMs =
        DEFAULT_FIREWORKS_MAX_LAUNCH_DELAY_MS;
  }
  if (config_.fireworksMaxLaunchDelayMs <
      config_.fireworksMinLaunchDelayMs) {
    config_.fireworksMaxLaunchDelayMs =
        config_.fireworksMinLaunchDelayMs;
  }
  config_.fireworksAnimSpeedMs = preferences.getUInt("fwAnimMs", 0);
  if (config_.fireworksAnimSpeedMs == 0) {
    config_.fireworksAnimSpeedMs = DEFAULT_FIREWORKS_ANIM_SPEED_MS;
  }

  config_.mazeMinWidth = preferences.getUInt("mzMinW", 0);
  if (config_.mazeMinWidth < MIN_MAZE_DIMENSION) {
    config_.mazeMinWidth = DEFAULT_MAZE_MIN_WIDTH;
  }
  config_.mazeMaxWidth = preferences.getUInt("mzMaxW", 0);
  if (config_.mazeMaxWidth < config_.mazeMinWidth) {
    config_.mazeMaxWidth = DEFAULT_MAZE_MAX_WIDTH;
  }
  if (config_.mazeMaxWidth < config_.mazeMinWidth) {
    config_.mazeMaxWidth = config_.mazeMinWidth;
  }
  config_.mazeMinHeight = preferences.getUInt("mzMinH", 0);
  if (config_.mazeMinHeight < MIN_MAZE_DIMENSION) {
    config_.mazeMinHeight = DEFAULT_MAZE_MIN_HEIGHT;
  }
  config_.mazeMaxHeight = preferences.getUInt("mzMaxH", 0);
  if (config_.mazeMaxHeight < config_.mazeMinHeight) {
    config_.mazeMaxHeight = DEFAULT_MAZE_MAX_HEIGHT;
  }
  if (config_.mazeMaxHeight < config_.mazeMinHeight) {
    config_.mazeMaxHeight = config_.mazeMinHeight;
  }
  config_.mazeHeroMinSpeedMs = preferences.getUInt("mzHeroMinMs", 0);
  if (config_.mazeHeroMinSpeedMs == 0) {
    config_.mazeHeroMinSpeedMs = DEFAULT_MAZE_HERO_MIN_SPEED_MS;
  }
  config_.mazeHeroMaxSpeedMs = preferences.getUInt("mzHeroMaxMs", 0);
  if (config_.mazeHeroMaxSpeedMs < config_.mazeHeroMinSpeedMs) {
    config_.mazeHeroMaxSpeedMs = DEFAULT_MAZE_HERO_MAX_SPEED_MS;
  }
  if (config_.mazeHeroMaxSpeedMs < config_.mazeHeroMinSpeedMs) {
    config_.mazeHeroMaxSpeedMs = config_.mazeHeroMinSpeedMs;
  }

  config_.brightness =
      preferences.getUChar("brightness", DEFAULT_DISPLAY_BRIGHTNESS);
  config_.fireworksMaxBrightness = preferences.getUChar(
      "fwMaxBright", DEFAULT_FIREWORKS_MAX_BRIGHTNESS);
  config_.weatherPostalCode = preferences.getString("wxZip", "");
  if (config_.weatherPostalCode.length() >
      MAX_WEATHER_POSTAL_CODE_LENGTH) {
    config_.weatherPostalCode =
        config_.weatherPostalCode.substring(0,
                                            MAX_WEATHER_POSTAL_CODE_LENGTH);
  }
  config_.flightLat = preferences.getFloat("fltLat", DEFAULT_FLIGHT_LAT);
  config_.flightLon = preferences.getFloat("fltLon", DEFAULT_FLIGHT_LON);
  config_.flightRadius =
      preferences.getFloat("fltRad", DEFAULT_FLIGHT_RADIUS);
  if (config_.flightRadius <= 0.0f) {
    config_.flightRadius = DEFAULT_FLIGHT_RADIUS;
  }
  config_.flightRadiusUnit =
      preferences.getUChar("fltRadUnit", FLIGHT_RADIUS_UNIT_MI);
  if (config_.flightRadiusUnit > FLIGHT_RADIUS_UNIT_KM) {
    config_.flightRadiusUnit = FLIGHT_RADIUS_UNIT_MI;
  }
  config_.flightSpeedUnit =
      preferences.getUChar("fltSpdUnit", FLIGHT_SPEED_UNIT_KT);
  if (config_.flightSpeedUnit > FLIGHT_SPEED_UNIT_KPH) {
    config_.flightSpeedUnit = FLIGHT_SPEED_UNIT_KT;
  }

  if (config_.brightness > 15) {
    config_.brightness = 15;
  }
  if (config_.fireworksMaxBrightness > 15) {
    config_.fireworksMaxBrightness = 15;
  }
  if (config_.fireworksMaxBrightness < config_.brightness) {
    config_.fireworksMaxBrightness = config_.brightness;
  }

  Serial.print(", program=");
  Serial.print(programIdToString(config_.program));
  Serial.print(", selectedPrograms=");
  Serial.print(selectedProgramsToString(config_.selectedPrograms));
  Serial.print(", programDurationMin=");
  Serial.print(config_.programDurationMinutes);
  Serial.print(", scrollMsg=");
  Serial.print(config_.scrollMessage.length() ? config_.scrollMessage
                                               : "(default)");
  Serial.print(", scrollSpeed=");
  Serial.print(config_.scrollSpeedMs);
  Serial.print(", fwMinDelayMs=");
  Serial.print(config_.fireworksMinLaunchDelayMs);
  Serial.print(", fwMaxDelayMs=");
  Serial.print(config_.fireworksMaxLaunchDelayMs);
  Serial.print(", fwAnimMs=");
  Serial.print(config_.fireworksAnimSpeedMs);
  Serial.print(", mazeWidth=");
  Serial.print(config_.mazeMinWidth);
  Serial.print("-");
  Serial.print(config_.mazeMaxWidth);
  Serial.print(", mazeHeight=");
  Serial.print(config_.mazeMinHeight);
  Serial.print("-");
  Serial.print(config_.mazeMaxHeight);
  Serial.print(", mazeHeroMs=");
  Serial.print(config_.mazeHeroMinSpeedMs);
  Serial.print("-");
  Serial.print(config_.mazeHeroMaxSpeedMs);
  Serial.print(", brightness=");
  Serial.print(config_.brightness);
  Serial.print(", fwMaxBright=");
  Serial.print(config_.fireworksMaxBrightness);
  Serial.print(", wxZip=");
  Serial.print(config_.weatherPostalCode.length() ? config_.weatherPostalCode
                                                   : "(empty)");
  Serial.print(", flight=");
  Serial.print(config_.flightLat, 4);
  Serial.print(",");
  Serial.print(config_.flightLon, 4);
  Serial.print(" r=");
  Serial.print(config_.flightRadius, 1);
  Serial.print(config_.flightRadiusUnit == FLIGHT_RADIUS_UNIT_KM ? "km"
                                                                 : "mi");
  Serial.print(" spd=");
  Serial.println(config_.flightSpeedUnit == FLIGHT_SPEED_UNIT_MPH
                     ? "mph"
                     : (config_.flightSpeedUnit == FLIGHT_SPEED_UNIT_KPH
                            ? "kph"
                            : "kt"));
}

void JustinProject::saveSettings(Preferences &preferences) const {
  preferences.putString("program", programIdToString(config_.program));
  preferences.putUChar(
      "programs",
      sanitizeSelectedPrograms(config_.selectedPrograms, config_.program));
  preferences.putFloat("progDurMin", config_.programDurationMinutes);
  preferences.putString("scrollMsg", config_.scrollMessage);
  preferences.putUInt("scrollSpeed", config_.scrollSpeedMs);
  preferences.putUInt("fwMinDelayMs", config_.fireworksMinLaunchDelayMs);
  preferences.putUInt("fwMaxDelayMs", config_.fireworksMaxLaunchDelayMs);
  preferences.putUInt("fwAnimMs", config_.fireworksAnimSpeedMs);
  preferences.putUInt("mzMinW", config_.mazeMinWidth);
  preferences.putUInt("mzMaxW", config_.mazeMaxWidth);
  preferences.putUInt("mzMinH", config_.mazeMinHeight);
  preferences.putUInt("mzMaxH", config_.mazeMaxHeight);
  preferences.putUInt("mzHeroMinMs", config_.mazeHeroMinSpeedMs);
  preferences.putUInt("mzHeroMaxMs", config_.mazeHeroMaxSpeedMs);
  preferences.putUChar("brightness", config_.brightness);
  preferences.putUChar("fwMaxBright", config_.fireworksMaxBrightness);
  preferences.putString("wxZip", config_.weatherPostalCode);
  preferences.putFloat("fltLat", config_.flightLat);
  preferences.putFloat("fltLon", config_.flightLon);
  preferences.putFloat("fltRad", config_.flightRadius);
  preferences.putUChar("fltRadUnit", config_.flightRadiusUnit);
  preferences.putUChar("fltSpdUnit", config_.flightSpeedUnit);
}

uint8_t JustinProject::displayBrightness() const {
  return config_.brightness;
}

String JustinProject::buildPortalPage(const String &ip,
                                      const String &ssid) const {
  String page = String(FPSTR(SETUP_PORTAL_HTML));
  page.replace("IP_PLACEHOLDER", ip);
  page.replace("SSID_PLACEHOLDER", htmlEscape(ssid));
  page.replace("PROGRAM_PLACEHOLDER",
               htmlEscape(programIdToString(config_.program)));
  page.replace(
      "SELECTED_PROGRAMS_PLACEHOLDER",
      htmlEscape(selectedProgramsToString(config_.selectedPrograms)));
  page.replace("PROGRAM_DURATION_PLACEHOLDER",
               String(config_.programDurationMinutes, 3));
  page.replace("SCROLL_MESSAGE_PLACEHOLDER",
               htmlEscape(config_.scrollMessage));
  page.replace("SCROLL_SPEED_PLACEHOLDER", String(config_.scrollSpeedMs));
  page.replace("FW_MIN_DELAY_PLACEHOLDER",
               String(config_.fireworksMinLaunchDelayMs));
  page.replace("FW_MAX_DELAY_PLACEHOLDER",
               String(config_.fireworksMaxLaunchDelayMs));
  page.replace("FW_ANIM_MS_PLACEHOLDER",
               String(config_.fireworksAnimSpeedMs));
  page.replace("MAZE_MIN_WIDTH_PLACEHOLDER", String(config_.mazeMinWidth));
  page.replace("MAZE_MAX_WIDTH_PLACEHOLDER", String(config_.mazeMaxWidth));
  page.replace("MAZE_MIN_HEIGHT_PLACEHOLDER", String(config_.mazeMinHeight));
  page.replace("MAZE_MAX_HEIGHT_PLACEHOLDER", String(config_.mazeMaxHeight));
  page.replace("MAZE_HERO_MIN_SPEED_PLACEHOLDER",
               String(config_.mazeHeroMinSpeedMs));
  page.replace("MAZE_HERO_MAX_SPEED_PLACEHOLDER",
               String(config_.mazeHeroMaxSpeedMs));
  page.replace("MIN_BRIGHTNESS_PLACEHOLDER", String(config_.brightness));
  page.replace("MAX_BRIGHTNESS_PLACEHOLDER",
               String(config_.fireworksMaxBrightness));
  page.replace("WEATHER_ZIP_PLACEHOLDER",
               htmlEscape(config_.weatherPostalCode));
  page.replace("FLIGHT_LAT_PLACEHOLDER", String(config_.flightLat, 5));
  page.replace("FLIGHT_LON_PLACEHOLDER", String(config_.flightLon, 5));
  page.replace("FLIGHT_RADIUS_PLACEHOLDER", String(config_.flightRadius, 1));
  page.replace("FLIGHT_RADIUS_UNIT_PLACEHOLDER",
               String(config_.flightRadiusUnit));
  page.replace("FLIGHT_SPEED_UNIT_PLACEHOLDER",
               String(config_.flightSpeedUnit));
  return page;
}

bool JustinProject::applyPortalRequest(const tinker::PortalRequest &request,
                                       String &error) {
  ProgramConfig newConfig = config_;
  ProgramId fallbackProgram = parseProgramId(request.arg("program"));
  uint8_t selectedPrograms =
      parseSelectedProgramsArg(request.arg("selectedPrograms"));
  if (request.arg("programScroller") == "1") {
    selectedPrograms |= PROGRAM_SCROLLER_FLAG;
  }
  if (request.arg("programFireworks") == "1") {
    selectedPrograms |= PROGRAM_FIREWORKS_FLAG;
  }
  if (request.arg("programMazeHero") == "1") {
    selectedPrograms |= PROGRAM_MAZE_HERO_FLAG;
  }
  if (request.arg("programPixelArt") == "1") {
    selectedPrograms |= PROGRAM_PIXEL_ART_FLAG;
  }
  if (request.arg("programWeatherWatch") == "1") {
    selectedPrograms |= PROGRAM_WEATHER_WATCH_FLAG;
  }
  if (request.arg("programRealWeather") == "1") {
    selectedPrograms |= PROGRAM_REAL_WEATHER_FLAG;
  }
  if (request.arg("programMoonPhase") == "1") {
    selectedPrograms |= PROGRAM_MOON_PHASE_FLAG;
  }
  if (request.arg("programFlightWatch") == "1") {
    selectedPrograms |= PROGRAM_FLIGHT_WATCH_FLAG;
  }
  selectedPrograms &= PROGRAM_ALL_FLAGS;
  if (selectedPrograms == 0) {
    error = "Select at least one program.";
    return false;
  }

  float programDurationMinutes =
      request.arg("programDurationMinutes").toFloat();
  if (programDurationMinutes <= 0.0f ||
      programDurationMinutes > MAX_PROGRAM_DURATION_MINUTES) {
    error = "Program duration must be greater than 0 and at most 43200.";
    return false;
  }

  newConfig.selectedPrograms =
      sanitizeSelectedPrograms(selectedPrograms, fallbackProgram);
  newConfig.program =
      firstSelectedProgram(newConfig.selectedPrograms, fallbackProgram);
  newConfig.programDurationMinutes = programDurationMinutes;

  int displayBrightness = request.arg("brightness").toInt();
  if (displayBrightness < 0) {
    displayBrightness = 0;
  }
  if (displayBrightness > 15) {
    displayBrightness = 15;
  }
  newConfig.brightness = static_cast<uint8_t>(displayBrightness);
  if (newConfig.fireworksMaxBrightness < newConfig.brightness) {
    newConfig.fireworksMaxBrightness = newConfig.brightness;
  }

  if (newConfig.selectedPrograms & PROGRAM_SCROLLER_FLAG) {
    String scrollMessage = request.arg("scrollMessage");
    scrollMessage.trim();
    if (scrollMessage.length() > MAX_SCROLL_MESSAGE_LENGTH) {
      error = "Scroll message is too long.";
      return false;
    }
    unsigned int scrollSpeedMs = request.arg("scrollSpeed").toInt();
    if (scrollSpeedMs <= 0) {
      error = "Scroll speed must be greater than 0.";
      return false;
    }
    newConfig.scrollMessage = scrollMessage;
    newConfig.scrollSpeedMs = scrollSpeedMs;
  }

  if (newConfig.selectedPrograms & PROGRAM_FIREWORKS_FLAG) {
    unsigned int fireworksMinLaunchDelayMs =
        request.arg("fireworksMinDelay").toInt();
    unsigned int fireworksMaxLaunchDelayMs =
        request.arg("fireworksMaxDelay").toInt();
    if (fireworksMinLaunchDelayMs <= 0) {
      error = "Min launch delay must be greater than 0.";
      return false;
    }
    if (fireworksMaxLaunchDelayMs < fireworksMinLaunchDelayMs) {
      error = "Max launch delay must be greater than or equal to min.";
      return false;
    }
    unsigned int fireworksAnimSpeedMs =
        request.arg("fireworksAnimMs").toInt();
    if (fireworksAnimSpeedMs <= 0) {
      error = "Animation speed must be greater than 0.";
      return false;
    }
    int fireworksMaxBrightness =
        request.arg("fireworksMaxBrightness").toInt();
    if (fireworksMaxBrightness < displayBrightness) {
      error = "Max brightness must be greater than or equal to min.";
      return false;
    }
    if (fireworksMaxBrightness > 15) {
      error = "Max brightness must be 0-15.";
      return false;
    }
    newConfig.fireworksMinLaunchDelayMs = fireworksMinLaunchDelayMs;
    newConfig.fireworksMaxLaunchDelayMs = fireworksMaxLaunchDelayMs;
    newConfig.fireworksAnimSpeedMs = fireworksAnimSpeedMs;
    newConfig.fireworksMaxBrightness =
        static_cast<uint8_t>(fireworksMaxBrightness);
  }

  if (newConfig.selectedPrograms & PROGRAM_MAZE_HERO_FLAG) {
    unsigned int mazeMinWidth = request.arg("mazeMinWidth").toInt();
    unsigned int mazeMaxWidth = request.arg("mazeMaxWidth").toInt();
    unsigned int mazeMinHeight = request.arg("mazeMinHeight").toInt();
    unsigned int mazeMaxHeight = request.arg("mazeMaxHeight").toInt();
    unsigned int mazeHeroMinSpeedMs =
        request.arg("mazeHeroMinSpeed").toInt();
    unsigned int mazeHeroMaxSpeedMs =
        request.arg("mazeHeroMaxSpeed").toInt();
    if (mazeMinWidth < MIN_MAZE_DIMENSION ||
        mazeMinHeight < MIN_MAZE_DIMENSION) {
      error = "Maze minimum size must be at least 4x4.";
      return false;
    }
    if (mazeMaxWidth < mazeMinWidth || mazeMaxHeight < mazeMinHeight) {
      error = "Maze max width/height must be greater than or equal to min.";
      return false;
    }
    if (mazeMaxWidth > MAX_MAZE_WIDTH || mazeMaxHeight > MAX_MAZE_HEIGHT) {
      error = "Maze max size is 80x48.";
      return false;
    }
    if (mazeHeroMinSpeedMs <= 0) {
      error = "Hero min speed must be greater than 0.";
      return false;
    }
    if (mazeHeroMaxSpeedMs < mazeHeroMinSpeedMs) {
      error = "Hero max speed must be greater than or equal to min.";
      return false;
    }
    newConfig.mazeMinWidth = mazeMinWidth;
    newConfig.mazeMaxWidth = mazeMaxWidth;
    newConfig.mazeMinHeight = mazeMinHeight;
    newConfig.mazeMaxHeight = mazeMaxHeight;
    newConfig.mazeHeroMinSpeedMs = mazeHeroMinSpeedMs;
    newConfig.mazeHeroMaxSpeedMs = mazeHeroMaxSpeedMs;
  }

  if (newConfig.selectedPrograms & PROGRAM_REAL_WEATHER_FLAG) {
    String weatherPostalCode = request.arg("weatherPostalCode");
    weatherPostalCode.trim();
    if (weatherPostalCode.length() > MAX_WEATHER_POSTAL_CODE_LENGTH) {
      error = "ZIP/postal code is too long.";
      return false;
    }
    newConfig.weatherPostalCode = weatherPostalCode;
  }

  if (newConfig.selectedPrograms & PROGRAM_FLIGHT_WATCH_FLAG) {
    float flightLat = request.arg("flightLat").toFloat();
    float flightLon = request.arg("flightLon").toFloat();
    float flightRadius = request.arg("flightRadius").toFloat();
    uint8_t flightRadiusUnit =
        static_cast<uint8_t>(request.arg("flightRadiusUnit").toInt());
    uint8_t flightSpeedUnit =
        static_cast<uint8_t>(request.arg("flightSpeedUnit").toInt());
    if (flightLat < -90.0f || flightLat > 90.0f) {
      error = "Latitude must be between -90 and 90.";
      return false;
    }
    if (flightLon < -180.0f || flightLon > 180.0f) {
      error = "Longitude must be between -180 and 180.";
      return false;
    }
    if (flightRadius <= 0.0f) {
      error = "Flight radius must be greater than 0.";
      return false;
    }
    if (flightRadiusUnit > FLIGHT_RADIUS_UNIT_KM) {
      flightRadiusUnit = FLIGHT_RADIUS_UNIT_MI;
    }
    if (flightSpeedUnit > FLIGHT_SPEED_UNIT_KPH) {
      flightSpeedUnit = FLIGHT_SPEED_UNIT_KT;
    }
    float radiusNm = flightRadiusUnit == FLIGHT_RADIUS_UNIT_KM
                         ? flightRadius * 0.539957f
                         : flightRadius * 0.868976f;
    if (radiusNm > MAX_FLIGHT_RADIUS_NM) {
      error = "Flight radius must be at most 250 nautical miles.";
      return false;
    }
    newConfig.flightLat = flightLat;
    newConfig.flightLon = flightLon;
    newConfig.flightRadius = flightRadius;
    newConfig.flightRadiusUnit = flightRadiusUnit;
    newConfig.flightSpeedUnit = flightSpeedUnit;
  }

  config_ = newConfig;
  return true;
}

void JustinProject::startPrograms() {
  config_.selectedPrograms =
      sanitizeSelectedPrograms(config_.selectedPrograms, config_.program);
  config_.program =
      firstSelectedProgram(config_.selectedPrograms, config_.program);
  scheduler_.begin(schedulerBindings(), config_.selectedPrograms,
                   static_cast<uint8_t>(config_.program),
                   programDurationMs(config_));
}

void JustinProject::tickPrograms() {
  if (!scheduler_.started()) {
    startPrograms();
  }
  scheduler_.tick(schedulerBindings());
}

void JustinProject::startTransition(void *) { transitionStartRandom(); }

bool JustinProject::tickTransition(void *) { return transitionTick(); }

uint32_t JustinProject::nowMs(void *) { return millis(); }

tinker::SchedulerBindings JustinProject::schedulerBindings() {
  tinker::SchedulerBindings bindings = {
      kPrograms, this, &JustinProject::nowMs, &JustinProject::startTransition,
      &JustinProject::tickTransition, PROGRAM_COUNT};
  return bindings;
}
