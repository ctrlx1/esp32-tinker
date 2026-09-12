#include <DNSServer.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <Preferences.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#ifdef WOKWI_SIM
#include <esp_phy_init.h>
#endif

#include "app_version.h"
#include "boot_network_display.h"
#include "programs/program.h"
#include "programs/transitions.h"
#include "setup_html.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 5

#define AP_SSID_PREFIX "ESP32-Tinker-Setup-"
#define WOKWI_GUEST_SSID "Wokwi-GUEST"
#define DNS_PORT 53
#define WIFI_TIMEOUT_MS 15000
#define WOKWI_SETUP_TIMEOUT_MS 8000

#ifdef WOKWI_SIM
const bool RUNNING_IN_WOKWI = true;
#else
const bool RUNNING_IN_WOKWI = false;
#endif

const unsigned int DEFAULT_SCROLL_SPEED_MS = 75;
const unsigned int DEFAULT_FIREWORKS_MIN_LAUNCH_DELAY_MS = 60;
const unsigned int DEFAULT_FIREWORKS_MAX_LAUNCH_DELAY_MS = 2000;
const unsigned int DEFAULT_FIREWORKS_ANIM_SPEED_MS = 60;
const unsigned int MIN_MAZE_DIMENSION = 4;
const unsigned int MAX_MAZE_WIDTH = 80;
const unsigned int MAX_MAZE_HEIGHT = 48;
const unsigned int DEFAULT_MAZE_MIN_WIDTH = 8;
const unsigned int DEFAULT_MAZE_MAX_WIDTH = 40;
const unsigned int DEFAULT_MAZE_MIN_HEIGHT = 4;
const unsigned int DEFAULT_MAZE_MAX_HEIGHT = 24;
const unsigned int DEFAULT_MAZE_HERO_MIN_SPEED_MS = 120;
const unsigned int DEFAULT_MAZE_HERO_MAX_SPEED_MS = 200;
const uint8_t DEFAULT_DISPLAY_BRIGHTNESS = 0;
const uint8_t DEFAULT_FIREWORKS_MAX_BRIGHTNESS = 15;

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

String saved_ssid, saved_pass;
ProgramConfig programConfig;
bool configMode = false;
bool captivePortalActive = false;
char setupMacSuffix[5] = "";
char setupScrollBuffer[48] = "";

struct ProgramScheduler {
  ProgramConfig cfg;
  bool started = false;
  bool transitionActive = false;
  unsigned long programStartMs = 0;
};

ProgramScheduler programScheduler;

void loadPrefs();
void savePrefs(const String &ssid, const String &pass,
               const ProgramConfig &cfg);
ProgramConfig buildProgramConfig();
uint8_t sanitizeSelectedPrograms(uint8_t selectedPrograms,
                                 ProgramId fallbackProgram);
uint8_t parseSelectedProgramsArg(const String &value);
ProgramId firstSelectedProgram(uint8_t selectedPrograms,
                               ProgramId fallbackProgram);
ProgramId nextSelectedProgram(uint8_t selectedPrograms,
                              ProgramId currentProgram);
bool hasMultipleSelectedPrograms(uint8_t selectedPrograms,
                                 ProgramId fallbackProgram);
unsigned long programDurationMs(const ProgramConfig &cfg);
void startProgramScheduler(const ProgramConfig &cfg);
void tickProgramScheduler();
String selectedProgramsToString(uint8_t selectedPrograms);
String html_escape(String value);
String buildPage();
void registerRoutes();
void applyDisplayBrightness();
void getSetupMacSuffix(char *suffix, size_t size);
String getSetupApSsid();
void initSetupDisplay();
void updateSetupDisplay();
#ifdef WOKWI_SIM
bool startWokwiStation();
#endif
void startConfigPortal();

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
  if (selectedPrograms & PROGRAM_SCROLLER_FLAG) {
    return ProgramId::Scroller;
  }
  if (selectedPrograms & PROGRAM_FIREWORKS_FLAG) {
    return ProgramId::Fireworks;
  }
  if (selectedPrograms & PROGRAM_MAZE_HERO_FLAG) {
    return ProgramId::MazeHero;
  }
  if (selectedPrograms & PROGRAM_PIXEL_ART_FLAG) {
    return ProgramId::PixelArt;
  }
  if (selectedPrograms & PROGRAM_WEATHER_WATCH_FLAG) {
    return ProgramId::WeatherWatch;
  }
  if (selectedPrograms & PROGRAM_REAL_WEATHER_FLAG) {
    return ProgramId::RealWeather;
  }
  if (selectedPrograms & PROGRAM_MOON_PHASE_FLAG) {
    return ProgramId::MoonPhase;
  }
  if (selectedPrograms & PROGRAM_FLIGHT_WATCH_FLAG) {
    return ProgramId::FlightWatch;
  }
  return fallbackProgram;
}

ProgramId nextSelectedProgram(uint8_t selectedPrograms,
                              ProgramId currentProgram) {
  selectedPrograms = sanitizeSelectedPrograms(selectedPrograms, currentProgram);
  constexpr uint8_t PROGRAM_COUNT = 8;
  ProgramId orderedPrograms[] = {
      ProgramId::Scroller,     ProgramId::Fireworks,    ProgramId::MazeHero,
      ProgramId::PixelArt,     ProgramId::WeatherWatch, ProgramId::RealWeather,
      ProgramId::MoonPhase,    ProgramId::FlightWatch};
  uint8_t currentIndex = 0;
  for (uint8_t i = 0; i < PROGRAM_COUNT; i++) {
    if (orderedPrograms[i] == currentProgram) {
      currentIndex = i;
      break;
    }
  }

  for (uint8_t offset = 1; offset <= PROGRAM_COUNT; offset++) {
    ProgramId candidate =
        orderedPrograms[(currentIndex + offset) % PROGRAM_COUNT];
    if (selectedPrograms & programIdToFlag(candidate)) {
      return candidate;
    }
  }
  return firstSelectedProgram(selectedPrograms, currentProgram);
}

bool hasMultipleSelectedPrograms(uint8_t selectedPrograms,
                                 ProgramId fallbackProgram) {
  selectedPrograms = sanitizeSelectedPrograms(selectedPrograms, fallbackProgram);
  uint8_t selectedCount = 0;
  if (selectedPrograms & PROGRAM_SCROLLER_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_FIREWORKS_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_MAZE_HERO_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_PIXEL_ART_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_WEATHER_WATCH_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_REAL_WEATHER_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_MOON_PHASE_FLAG) {
    selectedCount++;
  }
  if (selectedPrograms & PROGRAM_FLIGHT_WATCH_FLAG) {
    selectedCount++;
  }
  return selectedCount > 1;
}

unsigned long programDurationMs(const ProgramConfig &cfg) {
  float minutes = cfg.programDurationMinutes;
  if (minutes <= 0.0f) {
    minutes = DEFAULT_PROGRAM_DURATION_MINUTES;
  }
  if (minutes > MAX_PROGRAM_DURATION_MINUTES) {
    minutes = MAX_PROGRAM_DURATION_MINUTES;
  }

  double durationMs = (double)minutes * 60.0 * 1000.0;
  if (durationMs < 1.0) {
    return 1UL;
  }
  return (unsigned long)durationMs;
}

void startProgramScheduler(const ProgramConfig &cfg) {
  programScheduler.cfg = cfg;
  programScheduler.cfg.selectedPrograms =
      sanitizeSelectedPrograms(cfg.selectedPrograms, cfg.program);
  programScheduler.cfg.program =
      firstSelectedProgram(programScheduler.cfg.selectedPrograms, cfg.program);
  programScheduler.started = true;
  programScheduler.transitionActive = false;
  programScheduler.programStartMs = millis();
  programStart(programScheduler.cfg);
}

void tickProgramScheduler() {
  if (!programScheduler.started) {
    startProgramScheduler(buildProgramConfig());
  }

  unsigned long now = millis();
  if (programScheduler.transitionActive) {
    if (transitionTick()) {
      programScheduler.transitionActive = false;
      programScheduler.cfg.program = nextSelectedProgram(
          programScheduler.cfg.selectedPrograms, programScheduler.cfg.program);
      programScheduler.programStartMs = millis();
      programStart(programScheduler.cfg);
    }
    return;
  }

  programTick(programScheduler.cfg);
  if (hasMultipleSelectedPrograms(programScheduler.cfg.selectedPrograms,
                                  programScheduler.cfg.program) &&
      now - programScheduler.programStartMs >=
      programDurationMs(programScheduler.cfg)) {
    transitionStartRandom();
    programScheduler.transitionActive = true;
  }
}

String selectedProgramsToString(uint8_t selectedPrograms) {
  selectedPrograms =
      sanitizeSelectedPrograms(selectedPrograms, ProgramId::Scroller);
  String value;
  if (selectedPrograms & PROGRAM_SCROLLER_FLAG) {
    value += "scroller";
  }
  if (selectedPrograms & PROGRAM_FIREWORKS_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "fireworks";
  }
  if (selectedPrograms & PROGRAM_MAZE_HERO_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "maze_hero";
  }
  if (selectedPrograms & PROGRAM_PIXEL_ART_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "pixel_art";
  }
  if (selectedPrograms & PROGRAM_WEATHER_WATCH_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "weather_watch";
  }
  if (selectedPrograms & PROGRAM_REAL_WEATHER_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "real_weather";
  }
  if (selectedPrograms & PROGRAM_MOON_PHASE_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "moon_phase";
  }
  if (selectedPrograms & PROGRAM_FLIGHT_WATCH_FLAG) {
    if (value.length() > 0) {
      value += ",";
    }
    value += "flight_watch";
  }
  return value;
}

void loadPrefs() {
  prefs.begin("esp32tinker", true);
  saved_ssid = prefs.getString("ssid", "");
  saved_pass = prefs.getString("pass", "");
  programConfig.program =
      parseProgramId(prefs.getString("program", "scroller"));
  uint8_t savedPrograms = prefs.getUChar("programs", 0);
  programConfig.selectedPrograms =
      savedPrograms == 0
          ? DEFAULT_SELECTED_PROGRAMS
          : sanitizeSelectedPrograms(savedPrograms, programConfig.program);
  programConfig.program = firstSelectedProgram(programConfig.selectedPrograms,
                                               programConfig.program);
  programConfig.programDurationMinutes =
      prefs.getFloat("progDurMin", DEFAULT_PROGRAM_DURATION_MINUTES);
  if (programConfig.programDurationMinutes <= 0.0f ||
      programConfig.programDurationMinutes > MAX_PROGRAM_DURATION_MINUTES) {
    programConfig.programDurationMinutes = DEFAULT_PROGRAM_DURATION_MINUTES;
  }
  programConfig.scrollMessage = prefs.getString("scrollMsg", "");
  programConfig.scrollSpeedMs = prefs.getUInt("scrollSpeed", 0);
  if (programConfig.scrollSpeedMs == 0) {
    programConfig.scrollSpeedMs = DEFAULT_SCROLL_SPEED_MS;
  }
  programConfig.fireworksMinLaunchDelayMs = prefs.getUInt("fwMinDelayMs", 0);
  if (programConfig.fireworksMinLaunchDelayMs == 0) {
    programConfig.fireworksMinLaunchDelayMs =
        DEFAULT_FIREWORKS_MIN_LAUNCH_DELAY_MS;
  }
  programConfig.fireworksMaxLaunchDelayMs = prefs.getUInt("fwMaxDelayMs", 0);
  if (programConfig.fireworksMaxLaunchDelayMs == 0) {
    programConfig.fireworksMaxLaunchDelayMs =
        DEFAULT_FIREWORKS_MAX_LAUNCH_DELAY_MS;
  }
  if (programConfig.fireworksMaxLaunchDelayMs <
      programConfig.fireworksMinLaunchDelayMs) {
    programConfig.fireworksMaxLaunchDelayMs =
        programConfig.fireworksMinLaunchDelayMs;
  }
  programConfig.fireworksAnimSpeedMs = prefs.getUInt("fwAnimMs", 0);
  if (programConfig.fireworksAnimSpeedMs == 0) {
    programConfig.fireworksAnimSpeedMs = DEFAULT_FIREWORKS_ANIM_SPEED_MS;
  }
  programConfig.mazeMinWidth = prefs.getUInt("mzMinW", 0);
  if (programConfig.mazeMinWidth < MIN_MAZE_DIMENSION) {
    programConfig.mazeMinWidth = DEFAULT_MAZE_MIN_WIDTH;
  }
  programConfig.mazeMaxWidth = prefs.getUInt("mzMaxW", 0);
  if (programConfig.mazeMaxWidth < programConfig.mazeMinWidth) {
    programConfig.mazeMaxWidth = DEFAULT_MAZE_MAX_WIDTH;
  }
  if (programConfig.mazeMaxWidth < programConfig.mazeMinWidth) {
    programConfig.mazeMaxWidth = programConfig.mazeMinWidth;
  }
  programConfig.mazeMinHeight = prefs.getUInt("mzMinH", 0);
  if (programConfig.mazeMinHeight < MIN_MAZE_DIMENSION) {
    programConfig.mazeMinHeight = DEFAULT_MAZE_MIN_HEIGHT;
  }
  programConfig.mazeMaxHeight = prefs.getUInt("mzMaxH", 0);
  if (programConfig.mazeMaxHeight < programConfig.mazeMinHeight) {
    programConfig.mazeMaxHeight = DEFAULT_MAZE_MAX_HEIGHT;
  }
  if (programConfig.mazeMaxHeight < programConfig.mazeMinHeight) {
    programConfig.mazeMaxHeight = programConfig.mazeMinHeight;
  }
  programConfig.mazeHeroMinSpeedMs = prefs.getUInt("mzHeroMinMs", 0);
  if (programConfig.mazeHeroMinSpeedMs == 0) {
    programConfig.mazeHeroMinSpeedMs = DEFAULT_MAZE_HERO_MIN_SPEED_MS;
  }
  programConfig.mazeHeroMaxSpeedMs = prefs.getUInt("mzHeroMaxMs", 0);
  if (programConfig.mazeHeroMaxSpeedMs < programConfig.mazeHeroMinSpeedMs) {
    programConfig.mazeHeroMaxSpeedMs = DEFAULT_MAZE_HERO_MAX_SPEED_MS;
  }
  if (programConfig.mazeHeroMaxSpeedMs < programConfig.mazeHeroMinSpeedMs) {
    programConfig.mazeHeroMaxSpeedMs = programConfig.mazeHeroMinSpeedMs;
  }
  programConfig.brightness =
      prefs.getUChar("brightness", DEFAULT_DISPLAY_BRIGHTNESS);
  programConfig.fireworksMaxBrightness =
      prefs.getUChar("fwMaxBright", DEFAULT_FIREWORKS_MAX_BRIGHTNESS);
  programConfig.weatherPostalCode = prefs.getString("wxZip", "");
  if (programConfig.weatherPostalCode.length() >
      MAX_WEATHER_POSTAL_CODE_LENGTH) {
    programConfig.weatherPostalCode =
        programConfig.weatherPostalCode.substring(0,
                                                  MAX_WEATHER_POSTAL_CODE_LENGTH);
  }
  programConfig.flightLat = prefs.getFloat("fltLat", DEFAULT_FLIGHT_LAT);
  programConfig.flightLon = prefs.getFloat("fltLon", DEFAULT_FLIGHT_LON);
  programConfig.flightRadius = prefs.getFloat("fltRad", DEFAULT_FLIGHT_RADIUS);
  if (programConfig.flightRadius <= 0.0f) {
    programConfig.flightRadius = DEFAULT_FLIGHT_RADIUS;
  }
  programConfig.flightRadiusUnit =
      prefs.getUChar("fltRadUnit", FLIGHT_RADIUS_UNIT_MI);
  if (programConfig.flightRadiusUnit > FLIGHT_RADIUS_UNIT_KM) {
    programConfig.flightRadiusUnit = FLIGHT_RADIUS_UNIT_MI;
  }
  programConfig.flightSpeedUnit =
      prefs.getUChar("fltSpdUnit", FLIGHT_SPEED_UNIT_KT);
  if (programConfig.flightSpeedUnit > FLIGHT_SPEED_UNIT_KPH) {
    programConfig.flightSpeedUnit = FLIGHT_SPEED_UNIT_KT;
  }
  prefs.end();

  if (programConfig.brightness > 15) {
    programConfig.brightness = 15;
  }
  if (programConfig.fireworksMaxBrightness > 15) {
    programConfig.fireworksMaxBrightness = 15;
  }
  if (programConfig.fireworksMaxBrightness < programConfig.brightness) {
    programConfig.fireworksMaxBrightness = programConfig.brightness;
  }

  Serial.print("Config loaded: ssid=");
  Serial.print(saved_ssid.length() ? saved_ssid : "(empty)");
  Serial.print(", program=");
  Serial.print(programIdToString(programConfig.program));
  Serial.print(", selectedPrograms=");
  Serial.print(selectedProgramsToString(programConfig.selectedPrograms));
  Serial.print(", programDurationMin=");
  Serial.print(programConfig.programDurationMinutes);
  Serial.print(", scrollMsg=");
  Serial.print(programConfig.scrollMessage.length()
                   ? programConfig.scrollMessage
                   : "(default)");
  Serial.print(", scrollSpeed=");
  Serial.print(programConfig.scrollSpeedMs);
  Serial.print(", fwMinDelayMs=");
  Serial.print(programConfig.fireworksMinLaunchDelayMs);
  Serial.print(", fwMaxDelayMs=");
  Serial.print(programConfig.fireworksMaxLaunchDelayMs);
  Serial.print(", fwAnimMs=");
  Serial.print(programConfig.fireworksAnimSpeedMs);
  Serial.print(", mazeWidth=");
  Serial.print(programConfig.mazeMinWidth);
  Serial.print("-");
  Serial.print(programConfig.mazeMaxWidth);
  Serial.print(", mazeHeight=");
  Serial.print(programConfig.mazeMinHeight);
  Serial.print("-");
  Serial.print(programConfig.mazeMaxHeight);
  Serial.print(", mazeHeroMs=");
  Serial.print(programConfig.mazeHeroMinSpeedMs);
  Serial.print("-");
  Serial.print(programConfig.mazeHeroMaxSpeedMs);
  Serial.print(", brightness=");
  Serial.print(programConfig.brightness);
  Serial.print(", fwMaxBright=");
  Serial.print(programConfig.fireworksMaxBrightness);
  Serial.print(", wxZip=");
  Serial.print(programConfig.weatherPostalCode.length()
                   ? programConfig.weatherPostalCode
                   : "(empty)");
  Serial.print(", flight=");
  Serial.print(programConfig.flightLat, 4);
  Serial.print(",");
  Serial.print(programConfig.flightLon, 4);
  Serial.print(" r=");
  Serial.print(programConfig.flightRadius, 1);
  Serial.print(programConfig.flightRadiusUnit == FLIGHT_RADIUS_UNIT_KM
                   ? "km"
                   : "mi");
  Serial.print(" spd=");
  Serial.println(programConfig.flightSpeedUnit == FLIGHT_SPEED_UNIT_MPH
                     ? "mph"
                     : (programConfig.flightSpeedUnit == FLIGHT_SPEED_UNIT_KPH
                            ? "kph"
                            : "kt"));
}

void savePrefs(const String &ssid, const String &pass,
               const ProgramConfig &cfg) {
  prefs.begin("esp32tinker", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("program", programIdToString(cfg.program));
  prefs.putUChar("programs",
                 sanitizeSelectedPrograms(cfg.selectedPrograms, cfg.program));
  prefs.putFloat("progDurMin", cfg.programDurationMinutes);
  prefs.putString("scrollMsg", cfg.scrollMessage);
  prefs.putUInt("scrollSpeed", cfg.scrollSpeedMs);
  prefs.putUInt("fwMinDelayMs", cfg.fireworksMinLaunchDelayMs);
  prefs.putUInt("fwMaxDelayMs", cfg.fireworksMaxLaunchDelayMs);
  prefs.putUInt("fwAnimMs", cfg.fireworksAnimSpeedMs);
  prefs.putUInt("mzMinW", cfg.mazeMinWidth);
  prefs.putUInt("mzMaxW", cfg.mazeMaxWidth);
  prefs.putUInt("mzMinH", cfg.mazeMinHeight);
  prefs.putUInt("mzMaxH", cfg.mazeMaxHeight);
  prefs.putUInt("mzHeroMinMs", cfg.mazeHeroMinSpeedMs);
  prefs.putUInt("mzHeroMaxMs", cfg.mazeHeroMaxSpeedMs);
  prefs.putUChar("brightness", cfg.brightness);
  prefs.putUChar("fwMaxBright", cfg.fireworksMaxBrightness);
  prefs.putString("wxZip", cfg.weatherPostalCode);
  prefs.putFloat("fltLat", cfg.flightLat);
  prefs.putFloat("fltLon", cfg.flightLon);
  prefs.putFloat("fltRad", cfg.flightRadius);
  prefs.putUChar("fltRadUnit", cfg.flightRadiusUnit);
  prefs.putUChar("fltSpdUnit", cfg.flightSpeedUnit);
  prefs.end();
}

ProgramConfig buildProgramConfig() { return programConfig; }

String html_escape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

String buildPage() {
  String page = String(FPSTR(SETUP_PORTAL_HTML));
  String ip =
      configMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  page.replace("IP_PLACEHOLDER", ip);
  page.replace("SSID_PLACEHOLDER", html_escape(saved_ssid));
  page.replace("PROGRAM_PLACEHOLDER",
               html_escape(programIdToString(programConfig.program)));
  page.replace(
      "SELECTED_PROGRAMS_PLACEHOLDER",
      html_escape(selectedProgramsToString(programConfig.selectedPrograms)));
  page.replace("PROGRAM_DURATION_PLACEHOLDER",
               String(programConfig.programDurationMinutes, 3));
  page.replace("SCROLL_MESSAGE_PLACEHOLDER",
               html_escape(programConfig.scrollMessage));
  page.replace("SCROLL_SPEED_PLACEHOLDER", String(programConfig.scrollSpeedMs));
  page.replace("FW_MIN_DELAY_PLACEHOLDER",
               String(programConfig.fireworksMinLaunchDelayMs));
  page.replace("FW_MAX_DELAY_PLACEHOLDER",
               String(programConfig.fireworksMaxLaunchDelayMs));
  page.replace("FW_ANIM_MS_PLACEHOLDER",
               String(programConfig.fireworksAnimSpeedMs));
  page.replace("MAZE_MIN_WIDTH_PLACEHOLDER",
               String(programConfig.mazeMinWidth));
  page.replace("MAZE_MAX_WIDTH_PLACEHOLDER",
               String(programConfig.mazeMaxWidth));
  page.replace("MAZE_MIN_HEIGHT_PLACEHOLDER",
               String(programConfig.mazeMinHeight));
  page.replace("MAZE_MAX_HEIGHT_PLACEHOLDER",
               String(programConfig.mazeMaxHeight));
  page.replace("MAZE_HERO_MIN_SPEED_PLACEHOLDER",
               String(programConfig.mazeHeroMinSpeedMs));
  page.replace("MAZE_HERO_MAX_SPEED_PLACEHOLDER",
               String(programConfig.mazeHeroMaxSpeedMs));
  page.replace("MIN_BRIGHTNESS_PLACEHOLDER", String(programConfig.brightness));
  page.replace("MAX_BRIGHTNESS_PLACEHOLDER",
               String(programConfig.fireworksMaxBrightness));
  page.replace("WEATHER_ZIP_PLACEHOLDER",
               html_escape(programConfig.weatherPostalCode));
  page.replace("FLIGHT_LAT_PLACEHOLDER", String(programConfig.flightLat, 5));
  page.replace("FLIGHT_LON_PLACEHOLDER", String(programConfig.flightLon, 5));
  page.replace("FLIGHT_RADIUS_PLACEHOLDER",
               String(programConfig.flightRadius, 1));
  page.replace("FLIGHT_RADIUS_UNIT_PLACEHOLDER",
               String(programConfig.flightRadiusUnit));
  page.replace("FLIGHT_SPEED_UNIT_PLACEHOLDER",
               String(programConfig.flightSpeedUnit));
  return page;
}

void handleRoot() { server.send(200, "text/html", buildPage()); }

void handleSave() {
  ProgramConfig newConfig = programConfig;
  ProgramId fallbackProgram = parseProgramId(server.arg("program"));
  uint8_t selectedPrograms =
      parseSelectedProgramsArg(server.arg("selectedPrograms"));
  if (server.arg("programScroller") == "1") {
    selectedPrograms |= PROGRAM_SCROLLER_FLAG;
  }
  if (server.arg("programFireworks") == "1") {
    selectedPrograms |= PROGRAM_FIREWORKS_FLAG;
  }
  if (server.arg("programMazeHero") == "1") {
    selectedPrograms |= PROGRAM_MAZE_HERO_FLAG;
  }
  if (server.arg("programPixelArt") == "1") {
    selectedPrograms |= PROGRAM_PIXEL_ART_FLAG;
  }
  if (server.arg("programWeatherWatch") == "1") {
    selectedPrograms |= PROGRAM_WEATHER_WATCH_FLAG;
  }
  if (server.arg("programRealWeather") == "1") {
    selectedPrograms |= PROGRAM_REAL_WEATHER_FLAG;
  }
  if (server.arg("programMoonPhase") == "1") {
    selectedPrograms |= PROGRAM_MOON_PHASE_FLAG;
  }
  if (server.arg("programFlightWatch") == "1") {
    selectedPrograms |= PROGRAM_FLIGHT_WATCH_FLAG;
  }
  selectedPrograms &= PROGRAM_ALL_FLAGS;
  if (selectedPrograms == 0) {
    server.send(400, "text/plain", "Select at least one program.");
    return;
  }

  float programDurationMinutes = server.arg("programDurationMinutes").toFloat();
  if (programDurationMinutes <= 0.0f ||
      programDurationMinutes > MAX_PROGRAM_DURATION_MINUTES) {
    server.send(400, "text/plain",
                "Program duration must be greater than 0 and at most 43200.");
    return;
  }

  newConfig.selectedPrograms =
      sanitizeSelectedPrograms(selectedPrograms, fallbackProgram);
  newConfig.program =
      firstSelectedProgram(newConfig.selectedPrograms, fallbackProgram);
  newConfig.programDurationMinutes = programDurationMinutes;

  int displayBrightness = server.arg("brightness").toInt();
  if (displayBrightness < 0) {
    displayBrightness = 0;
  }
  if (displayBrightness > 15) {
    displayBrightness = 15;
  }
  newConfig.brightness = (uint8_t)displayBrightness;
  if (newConfig.fireworksMaxBrightness < newConfig.brightness) {
    newConfig.fireworksMaxBrightness = newConfig.brightness;
  }

  if (newConfig.selectedPrograms & PROGRAM_SCROLLER_FLAG) {
    String scrollMessage = server.arg("scrollMessage");
    scrollMessage.trim();
    if (scrollMessage.length() > MAX_SCROLL_MESSAGE_LENGTH) {
      server.send(400, "text/plain", "Scroll message is too long.");
      return;
    }

    unsigned int scrollSpeedMs = server.arg("scrollSpeed").toInt();
    if (scrollSpeedMs <= 0) {
      server.send(400, "text/plain", "Scroll speed must be greater than 0.");
      return;
    }

    newConfig.scrollMessage = scrollMessage;
    newConfig.scrollSpeedMs = scrollSpeedMs;
  }
  if (newConfig.selectedPrograms & PROGRAM_FIREWORKS_FLAG) {
    unsigned int fireworksMinLaunchDelayMs =
        server.arg("fireworksMinDelay").toInt();
    unsigned int fireworksMaxLaunchDelayMs =
        server.arg("fireworksMaxDelay").toInt();
    if (fireworksMinLaunchDelayMs <= 0) {
      server.send(400, "text/plain",
                  "Min launch delay must be greater than 0.");
      return;
    }
    if (fireworksMaxLaunchDelayMs < fireworksMinLaunchDelayMs) {
      server.send(400, "text/plain",
                  "Max launch delay must be greater than or equal to min.");
      return;
    }

    unsigned int fireworksAnimSpeedMs = server.arg("fireworksAnimMs").toInt();
    if (fireworksAnimSpeedMs <= 0) {
      server.send(400, "text/plain", "Animation speed must be greater than 0.");
      return;
    }

    int fireworksMaxBrightness = server.arg("fireworksMaxBrightness").toInt();
    if (fireworksMaxBrightness < displayBrightness) {
      server.send(400, "text/plain",
                  "Max brightness must be greater than or equal to min.");
      return;
    }
    if (fireworksMaxBrightness > 15) {
      server.send(400, "text/plain", "Max brightness must be 0-15.");
      return;
    }

    newConfig.fireworksMinLaunchDelayMs = fireworksMinLaunchDelayMs;
    newConfig.fireworksMaxLaunchDelayMs = fireworksMaxLaunchDelayMs;
    newConfig.fireworksAnimSpeedMs = fireworksAnimSpeedMs;
    newConfig.fireworksMaxBrightness = (uint8_t)fireworksMaxBrightness;
  }
  if (newConfig.selectedPrograms & PROGRAM_MAZE_HERO_FLAG) {
    unsigned int mazeMinWidth = server.arg("mazeMinWidth").toInt();
    unsigned int mazeMaxWidth = server.arg("mazeMaxWidth").toInt();
    unsigned int mazeMinHeight = server.arg("mazeMinHeight").toInt();
    unsigned int mazeMaxHeight = server.arg("mazeMaxHeight").toInt();
    unsigned int mazeHeroMinSpeedMs = server.arg("mazeHeroMinSpeed").toInt();
    unsigned int mazeHeroMaxSpeedMs = server.arg("mazeHeroMaxSpeed").toInt();

    if (mazeMinWidth < MIN_MAZE_DIMENSION ||
        mazeMinHeight < MIN_MAZE_DIMENSION) {
      server.send(400, "text/plain", "Maze minimum size must be at least 4x4.");
      return;
    }
    if (mazeMaxWidth < mazeMinWidth || mazeMaxHeight < mazeMinHeight) {
      server.send(
          400, "text/plain",
          "Maze max width/height must be greater than or equal to min.");
      return;
    }
    if (mazeMaxWidth > MAX_MAZE_WIDTH || mazeMaxHeight > MAX_MAZE_HEIGHT) {
      server.send(400, "text/plain", "Maze max size is 80x48.");
      return;
    }
    if (mazeHeroMinSpeedMs <= 0) {
      server.send(400, "text/plain", "Hero min speed must be greater than 0.");
      return;
    }
    if (mazeHeroMaxSpeedMs < mazeHeroMinSpeedMs) {
      server.send(400, "text/plain",
                  "Hero max speed must be greater than or equal to min.");
      return;
    }

    newConfig.mazeMinWidth = mazeMinWidth;
    newConfig.mazeMaxWidth = mazeMaxWidth;
    newConfig.mazeMinHeight = mazeMinHeight;
    newConfig.mazeMaxHeight = mazeMaxHeight;
    newConfig.mazeHeroMinSpeedMs = mazeHeroMinSpeedMs;
    newConfig.mazeHeroMaxSpeedMs = mazeHeroMaxSpeedMs;
  }
  if (newConfig.selectedPrograms & PROGRAM_REAL_WEATHER_FLAG) {
    String weatherPostalCode = server.arg("weatherPostalCode");
    weatherPostalCode.trim();
    if (weatherPostalCode.length() > MAX_WEATHER_POSTAL_CODE_LENGTH) {
      server.send(400, "text/plain", "ZIP/postal code is too long.");
      return;
    }
    newConfig.weatherPostalCode = weatherPostalCode;
  }
  if (newConfig.selectedPrograms & PROGRAM_FLIGHT_WATCH_FLAG) {
    float flightLat = server.arg("flightLat").toFloat();
    float flightLon = server.arg("flightLon").toFloat();
    float flightRadius = server.arg("flightRadius").toFloat();
    uint8_t flightRadiusUnit =
        (uint8_t)server.arg("flightRadiusUnit").toInt();
    uint8_t flightSpeedUnit =
        (uint8_t)server.arg("flightSpeedUnit").toInt();
    if (flightLat < -90.0f || flightLat > 90.0f) {
      server.send(400, "text/plain", "Latitude must be between -90 and 90.");
      return;
    }
    if (flightLon < -180.0f || flightLon > 180.0f) {
      server.send(400, "text/plain", "Longitude must be between -180 and 180.");
      return;
    }
    if (flightRadius <= 0.0f) {
      server.send(400, "text/plain", "Flight radius must be greater than 0.");
      return;
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
      server.send(400, "text/plain",
                  "Flight radius must be at most 250 nautical miles.");
      return;
    }
    newConfig.flightLat = flightLat;
    newConfig.flightLon = flightLon;
    newConfig.flightRadius = flightRadius;
    newConfig.flightRadiusUnit = flightRadiusUnit;
    newConfig.flightSpeedUnit = flightSpeedUnit;
  }

  String new_ssid = server.arg("ssid");
  new_ssid.trim();
  if (new_ssid.length() == 0) {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == WOKWI_GUEST_SSID) {
      new_ssid = RUNNING_IN_WOKWI ? "" : String(WOKWI_GUEST_SSID);
    } else {
      new_ssid = saved_ssid;
    }
  }
  String new_pass = server.arg("pass");
  if (new_pass.length() == 0) {
    new_pass = saved_pass;
  }
  if (RUNNING_IN_WOKWI && new_ssid == WOKWI_GUEST_SSID) {
    new_ssid = "";
    new_pass = "";
  }

  savePrefs(new_ssid, new_pass, newConfig);
  server.send(200, "text/plain", "Saved! Rebooting now...");
  delay(1500);
  ESP.restart();
}

void handleUpdate() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA start: %s\n", upload.filename.c_str());
    Display.print("OTA...");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA success: %u bytes\n", upload.totalSize);
      Display.print("Rebooting");
    } else {
      Update.printError(Serial);
      Display.print("OTA fail");
    }
  }
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) {
      json += ",";
    }
    json += "{\"ssid\":\"" + WiFi.SSID(i) +
            "\","
            "\"rssi\":" +
            String(WiFi.RSSI(i)) +
            ","
            "\"secure\":" +
            (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleUpdateFinish() {
  if (Update.hasError()) {
    server.send(500, "text/plain", Update.errorString());
  } else {
    server.send(200, "text/plain", "OK");
    delay(1000);
    ESP.restart();
  }
}

void registerRoutes() {
  server.on("/", handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdate);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
}

void applyDisplayBrightness() {
  Display.setIntensity(programConfig.brightness);
}

void getSetupMacSuffix(char *suffix, size_t size) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(suffix, size, "%02X%02X", mac[4], mac[5]);
}

String getSetupApSsid() {
  char suffix[5];
  getSetupMacSuffix(suffix, sizeof(suffix));
  return String(AP_SSID_PREFIX) + suffix;
}

void initSetupDisplay() {
  getSetupMacSuffix(setupMacSuffix, sizeof(setupMacSuffix));
  snprintf(setupScrollBuffer, sizeof(setupScrollBuffer),
           "Connect to hotspot %s%s", AP_SSID_PREFIX, setupMacSuffix);
  Display.displayClear();
  Display.displayScroll(setupScrollBuffer, PA_LEFT, PA_SCROLL_LEFT, 80);
}

void updateSetupDisplay() {
  if (Display.displayAnimate()) {
    Display.displayReset();
  }
}

#ifdef WOKWI_SIM
bool startWokwiStation() {
  // Stale PHY cal data in the sim NVS triggers a loud MAC mismatch log;
  // erase it so WiFi init does a clean full calibration.
  esp_phy_erase_cal_data_in_nvs();

  Display.print(" Wokwi...");
  Serial.println("Connecting to Wokwi-GUEST...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WOKWI_GUEST_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WOKWI_SETUP_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wokwi-GUEST not found.");
    return false;
  }

  // Normal operation in the sim: programs + config page on STA.
  configMode = false;
  captivePortalActive = false;
  registerRoutes();
  server.begin();
  startProgramScheduler(buildProgramConfig());

  Serial.println("Wokwi ready.");
  Serial.println("Open http://localhost:8180 in your browser.");
  Serial.print("ESP IP: ");
  Serial.println(WiFi.localIP());
  return true;
}
#endif

void startConfigPortal() {
  configMode = true;

  WiFi.mode(WIFI_AP);
  String setupApSsid = getSetupApSsid();
  WiFi.softAP(setupApSsid.c_str());
  delay(500);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  captivePortalActive = true;
  registerRoutes();
  server.begin();

  Serial.println("Config portal started.");
  Serial.print("AP SSID: ");
  Serial.println(setupApSsid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  initSetupDisplay();
}

bool connectToSavedWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(saved_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());

  startProgramScheduler(buildProgramConfig());

  unsigned long start = millis();
  unsigned long lastDotMs = start;
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    tickProgramScheduler();
    if (millis() - lastDotMs >= 500) {
      Serial.print(".");
      lastDotMs = millis();
    }
    delay(10);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi failed.");
    return false;
  }

  Serial.println("");
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  if (!RUNNING_IN_WOKWI) {
    showBootIpAddress(Display, WiFi.localIP());
    startProgramScheduler(buildProgramConfig());
  }
  return true;
}

void setup() {
  Serial.begin(9600);

  Display.begin();
  Display.setIntensity(0);
  Display.setTextAlignment(PA_CENTER);
  showBootVersion(Display);

  loadPrefs();
  applyDisplayBrightness();

#ifdef WOKWI_SIM
  // Simulator always uses Wokwi-GUEST; never attempt saved SSIDs.
  if (!startWokwiStation()) {
    startConfigPortal();
  }
#else
  if (saved_ssid.length() > 0) {
    if (connectToSavedWiFi()) {
      registerRoutes();
      server.begin();
    } else {
      startConfigPortal();
    }
  } else {
    Serial.println("No credentials. Starting config portal.");
    startConfigPortal();
  }
#endif
}

void loop() {
  if (configMode) {
    if (captivePortalActive) {
      dnsServer.processNextRequest();
    }
    server.handleClient();
    updateSetupDisplay();
    return;
  }

  server.handleClient();
  tickProgramScheduler();
}
