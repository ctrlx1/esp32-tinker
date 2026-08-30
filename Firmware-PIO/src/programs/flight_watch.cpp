#include "flight_watch.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include <pgmspace.h>

namespace {
constexpr uint8_t MAX_AIRCRAFT = 3;
constexpr unsigned long REFRESH_INTERVAL_MS = 45UL * 1000UL;
constexpr unsigned long RETRY_INTERVAL_MS = 20UL * 1000UL;
constexpr int VERT_RATE_LEVEL_FPM = 100;
constexpr float MAX_RADIUS_NM = 250.0f;
constexpr float MI_TO_NM = 0.868976f;
constexpr float KM_TO_NM = 0.539957f;

struct Aircraft {
  char callsign[9];
  char typeCode[5];
  int16_t altFt;
  int16_t gsKt;
  int16_t trackDeg;
  int16_t vertRateFpm;
  bool hasVertRate;
  float dstNm;
};

Aircraft aircraftQueue[MAX_AIRCRAFT];
uint8_t aircraftCount = 0;
uint8_t aircraftIndex = 0;
unsigned long lastFetchMs = 0;
bool fetchSucceeded = false;
float lastLat = 0;
float lastLon = 0;
float lastRadius = 0;
uint8_t lastRadiusUnit = 0;

bool airlineNameFromCallsign(const char *callsign, char *out, size_t outSize) {
  struct AirlineEntry {
    char icao[4];
    char name[12];
  };
  static const AirlineEntry airlines[] PROGMEM = {
      {"AAL", "American"},  {"UAL", "United"},   {"DAL", "Delta"},
      {"SWA", "Southwest"}, {"JBU", "JetBlue"},  {"ASA", "Alaska"},
      {"FFT", "Frontier"},  {"NKS", "Spirit"},   {"HAL", "Hawaiian"},
      {"BAW", "British"},   {"AFR", "AirFrance"},{"DLH", "Lufthansa"},
      {"UAE", "Emirates"},  {"ACA", "AirCanada"},{"SKW", "SkyWest"},
      {"ENY", "Envoy"},     {"RPA", "RepubAir"}, {"EDV", "Endeavor"},
      {"FDX", "FedEx"},     {"UPS", "UPS"},      {"KLM", "KLM"},
      {"IBE", "Iberia"},    {"RYR", "Ryanair"},  {"VIR", "Virgin"},
      {"QFA", "Qantas"},
  };

  if (outSize == 0 || callsign == nullptr || strlen(callsign) < 3) {
    return false;
  }
  char prefix[4] = {callsign[0], callsign[1], callsign[2], '\0'};
  for (int i = 0; i < 3; i++) {
    if (prefix[i] >= 'a' && prefix[i] <= 'z') {
      prefix[i] = (char)(prefix[i] - 'a' + 'A');
    }
  }

  AirlineEntry entry;
  for (size_t i = 0; i < sizeof(airlines) / sizeof(airlines[0]); i++) {
    memcpy_P(&entry, &airlines[i], sizeof(entry));
    if (strcmp(prefix, entry.icao) == 0) {
      strncpy(out, entry.name, outSize - 1);
      out[outSize - 1] = '\0';
      return true;
    }
  }
  return false;
}

bool extractNumberAfterKey(const String &json, const char *key, float &out,
                           int fromIndex, int toIndex) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle, fromIndex);
  if (keyPos < 0 || (toIndex >= 0 && keyPos >= toIndex)) {
    return false;
  }
  int valuePos = keyPos + needle.length();
  while (valuePos < (int)json.length() &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    valuePos++;
  }
  if (valuePos < (int)json.length() && json.charAt(valuePos) == '"') {
    // String values like "ground" are not numeric.
    return false;
  }
  out = json.substring(valuePos).toFloat();
  return true;
}

bool extractStringAfterKey(const String &json, const char *key, String &out,
                           int fromIndex, int toIndex) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle, fromIndex);
  if (keyPos < 0 || (toIndex >= 0 && keyPos >= toIndex)) {
    return false;
  }
  int valuePos = keyPos + needle.length();
  while (valuePos < (int)json.length() &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    valuePos++;
  }
  if (valuePos >= (int)json.length() || json.charAt(valuePos) != '"') {
    return false;
  }
  int endQuote = json.indexOf('"', valuePos + 1);
  if (endQuote < 0 || (toIndex >= 0 && endQuote > toIndex)) {
    return false;
  }
  out = json.substring(valuePos + 1, endQuote);
  out.trim();
  return true;
}

bool isGroundAltitude(const String &json, int fromIndex, int toIndex) {
  String needle = "\"alt_baro\":";
  int keyPos = json.indexOf(needle, fromIndex);
  if (keyPos < 0 || (toIndex >= 0 && keyPos >= toIndex)) {
    return false;
  }
  int valuePos = keyPos + needle.length();
  while (valuePos < (int)json.length() &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    valuePos++;
  }
  return valuePos + 8 <= (int)json.length() &&
         json.substring(valuePos, valuePos + 8) == "\"ground\"";
}

void copyCapped(char *dest, size_t destSize, const String &src) {
  if (destSize == 0) {
    return;
  }
  size_t n = src.length();
  if (n >= destSize) {
    n = destSize - 1;
  }
  memcpy(dest, src.c_str(), n);
  dest[n] = '\0';
}

float radiusToNm(float radius, uint8_t unit) {
  if (unit == FLIGHT_RADIUS_UNIT_KM) {
    return radius * KM_TO_NM;
  }
  return radius * MI_TO_NM;
}

void setScrollText(const char *text) {
  strncpy(gProgramScrollBuffer, text, PROGRAM_SCROLL_BUFFER_SIZE - 1);
  gProgramScrollBuffer[PROGRAM_SCROLL_BUFFER_SIZE - 1] = '\0';
}

void startScroll(const ProgramConfig &cfg) {
  Display.displayClear();
  Display.setIntensity(cfg.brightness > 15 ? 15 : cfg.brightness);
  Display.setTextAlignment(PA_LEFT);
  unsigned int speed = cfg.scrollSpeedMs > 0 ? cfg.scrollSpeedMs : 75U;
  Display.displayScroll(gProgramScrollBuffer, PA_LEFT, PA_SCROLL_LEFT, speed);
}

void buildAircraftScroll(const Aircraft &ac) {
  String message;
  char airline[12];
  if (airlineNameFromCallsign(ac.callsign, airline, sizeof(airline))) {
    message += airline;
    message += " ";
  }
  if (ac.callsign[0] != '\0') {
    message += ac.callsign;
  } else {
    message += "Aircraft";
  }
  if (ac.typeCode[0] != '\0') {
    message += " ";
    message += ac.typeCode;
  }
  message += " ";
  message += String(ac.altFt);
  message += "ft ";
  message += String(ac.gsKt);
  message += "kt ";
  char trackBuf[8];
  snprintf(trackBuf, sizeof(trackBuf), "%03d", ac.trackDeg % 360);
  message += trackBuf;
  message += "deg";
  if (ac.hasVertRate) {
    message += " ";
    message += String(ac.vertRateFpm);
    message += "fpm ";
    if (ac.vertRateFpm >= VERT_RATE_LEVEL_FPM) {
      message += "CLIMB";
    } else if (ac.vertRateFpm <= -VERT_RATE_LEVEL_FPM) {
      message += "DESC";
    } else {
      message += "LEVEL";
    }
  }

  if (message.length() >= PROGRAM_SCROLL_BUFFER_SIZE) {
    message = message.substring(0, PROGRAM_SCROLL_BUFFER_SIZE - 1);
  }
  setScrollText(message.c_str());
}

void showCurrentOrEmpty(const ProgramConfig &cfg) {
  if (aircraftCount == 0) {
    setScrollText(fetchSucceeded ? "No aircraft nearby" : "Flight fetch failed");
  } else {
    if (aircraftIndex >= aircraftCount) {
      aircraftIndex = 0;
    }
    buildAircraftScroll(aircraftQueue[aircraftIndex]);
  }
  startScroll(cfg);
}

void insertSorted(const Aircraft &candidate) {
  uint8_t insertAt = aircraftCount;
  for (uint8_t i = 0; i < aircraftCount; i++) {
    if (candidate.dstNm < aircraftQueue[i].dstNm) {
      insertAt = i;
      break;
    }
  }
  if (aircraftCount < MAX_AIRCRAFT) {
    for (uint8_t i = aircraftCount; i > insertAt; i--) {
      aircraftQueue[i] = aircraftQueue[i - 1];
    }
    aircraftQueue[insertAt] = candidate;
    aircraftCount++;
    return;
  }
  if (insertAt >= MAX_AIRCRAFT) {
    return;
  }
  for (uint8_t i = MAX_AIRCRAFT - 1; i > insertAt; i--) {
    aircraftQueue[i] = aircraftQueue[i - 1];
  }
  aircraftQueue[insertAt] = candidate;
}

bool parseAircraftList(const String &json) {
  aircraftCount = 0;
  aircraftIndex = 0;

  int acPos = json.indexOf("\"ac\"");
  if (acPos < 0) {
    return false;
  }
  int arrayStart = json.indexOf('[', acPos);
  if (arrayStart < 0) {
    return false;
  }

  int searchFrom = arrayStart;
  while (true) {
    int hexPos = json.indexOf("\"hex\":", searchFrom);
    if (hexPos < 0) {
      break;
    }
    int nextHex = json.indexOf("\"hex\":", hexPos + 6);
    int toIndex = nextHex < 0 ? json.length() : nextHex;

    if (isGroundAltitude(json, hexPos, toIndex)) {
      searchFrom = hexPos + 6;
      continue;
    }

    Aircraft ac = {};
    ac.dstNm = 9999.0f;
    ac.hasVertRate = false;

    String flight;
    String typeCode;
    float alt = 0;
    float gs = 0;
    float track = 0;
    float vert = 0;
    float dst = 0;
    bool hasAlt = extractNumberAfterKey(json, "alt_baro", alt, hexPos, toIndex);
    if (!hasAlt) {
      searchFrom = hexPos + 6;
      continue;
    }

    extractStringAfterKey(json, "flight", flight, hexPos, toIndex);
    extractStringAfterKey(json, "t", typeCode, hexPos, toIndex);
    extractNumberAfterKey(json, "gs", gs, hexPos, toIndex);
    if (!extractNumberAfterKey(json, "track", track, hexPos, toIndex)) {
      extractNumberAfterKey(json, "true_heading", track, hexPos, toIndex);
    }
    if (extractNumberAfterKey(json, "baro_rate", vert, hexPos, toIndex) ||
        extractNumberAfterKey(json, "geom_rate", vert, hexPos, toIndex)) {
      ac.hasVertRate = true;
      ac.vertRateFpm = (int16_t)lroundf(vert);
    }
    extractNumberAfterKey(json, "dst", dst, hexPos, toIndex);

    copyCapped(ac.callsign, sizeof(ac.callsign), flight);
    copyCapped(ac.typeCode, sizeof(ac.typeCode), typeCode);
    ac.altFt = (int16_t)lroundf(alt);
    ac.gsKt = (int16_t)lroundf(gs);
    int trackInt = (int)lroundf(track) % 360;
    if (trackInt < 0) {
      trackInt += 360;
    }
    ac.trackDeg = (int16_t)trackInt;
    ac.dstNm = dst;

    insertSorted(ac);
    searchFrom = hexPos + 6;
  }

  return true;
}

bool httpGetCapped(const String &url, String &body, size_t maxBytes) {
  body = "";
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Flight watch: WiFi not connected");
    return false;
  }

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("ESP32-Tinker-FlightWatch");
  if (!http.begin(url)) {
    Serial.println("Flight watch: http.begin failed");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Flight watch HTTP %d for %s\n", code, url.c_str());
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    Serial.println("Flight watch: no response stream");
    http.end();
    return false;
  }

  // Cap body size to avoid heap exhaustion near busy airports.
  body.reserve(maxBytes > 64 ? maxBytes : 64);
  unsigned long idleStart = millis();
  while (http.connected() && body.length() < maxBytes) {
    size_t available = stream->available();
    if (available == 0) {
      if (contentLength > 0 && (int)body.length() >= contentLength) {
        break;
      }
      if (millis() - idleStart > 4000) {
        break;
      }
      delay(1);
      continue;
    }
    idleStart = millis();
    while (available-- > 0 && body.length() < maxBytes) {
      int c = stream->read();
      if (c < 0) {
        break;
      }
      body += (char)c;
    }
  }

  http.end();
  Serial.printf("Flight watch got %u bytes (cap %u)\n",
                (unsigned)body.length(), (unsigned)maxBytes);
  return body.length() > 0;
}

bool refreshFlights(const ProgramConfig &cfg) {
  float radiusNm = radiusToNm(cfg.flightRadius, cfg.flightRadiusUnit);
  if (cfg.flightRadius <= 0.0f || radiusNm <= 0.0f) {
    setScrollText("Set location in setup");
    aircraftCount = 0;
    fetchSucceeded = false;
    return false;
  }
  if (radiusNm > MAX_RADIUS_NM) {
    radiusNm = MAX_RADIUS_NM;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setScrollText("Flight unavailable");
    aircraftCount = 0;
    fetchSucceeded = false;
    return false;
  }

  // Prefer a modest radius for the request when the UI radius is huge — still
  // honors cfg up to API max, but keep path formatting explicit for negatives.
  char urlBuf[128];
  snprintf(urlBuf, sizeof(urlBuf),
           "http://api.adsb.lol/v2/lat/%.5f/lon/%.5f/dist/%.2f", cfg.flightLat,
           cfg.flightLon, radiusNm);

  Serial.print("Flight watch fetch: ");
  Serial.println(urlBuf);

  String body;
  constexpr size_t MAX_BODY_BYTES = 8192;
  if (!httpGetCapped(urlBuf, body, MAX_BODY_BYTES)) {
    // Fallback: single closest aircraft (much smaller payload).
    snprintf(urlBuf, sizeof(urlBuf),
             "http://api.adsb.lol/v2/closest/%.5f/%.5f/%.2f", cfg.flightLat,
             cfg.flightLon, radiusNm);
    Serial.print("Flight watch fallback: ");
    Serial.println(urlBuf);
    if (!httpGetCapped(urlBuf, body, MAX_BODY_BYTES)) {
      setScrollText("Flight fetch failed");
      aircraftCount = 0;
      fetchSucceeded = false;
      return false;
    }
  }

  if (!parseAircraftList(body)) {
    setScrollText("Flight parse failed");
    aircraftCount = 0;
    fetchSucceeded = false;
    return false;
  }

  fetchSucceeded = true;
  aircraftIndex = 0;
  return true;
}
} // namespace

void flightWatchStart(const ProgramConfig &cfg) {
  lastLat = cfg.flightLat;
  lastLon = cfg.flightLon;
  lastRadius = cfg.flightRadius;
  lastRadiusUnit = cfg.flightRadiusUnit;
  lastFetchMs = 0;
  aircraftCount = 0;
  aircraftIndex = 0;
  setScrollText("Loading flights...");
  startScroll(cfg);

  if (refreshFlights(cfg)) {
    lastFetchMs = millis();
  } else {
    lastFetchMs = millis();
  }
  showCurrentOrEmpty(cfg);
}

void flightWatchTick(const ProgramConfig &cfg) {
  unsigned long now = millis();
  unsigned long interval =
      fetchSucceeded ? REFRESH_INTERVAL_MS : RETRY_INTERVAL_MS;
  bool configChanged = cfg.flightLat != lastLat || cfg.flightLon != lastLon ||
                       cfg.flightRadius != lastRadius ||
                       cfg.flightRadiusUnit != lastRadiusUnit;

  if (configChanged || now - lastFetchMs >= interval) {
    lastLat = cfg.flightLat;
    lastLon = cfg.flightLon;
    lastRadius = cfg.flightRadius;
    lastRadiusUnit = cfg.flightRadiusUnit;
    refreshFlights(cfg);
    lastFetchMs = now;
    showCurrentOrEmpty(cfg);
    return;
  }

  if (Display.displayAnimate()) {
    if (aircraftCount > 0) {
      aircraftIndex = (uint8_t)((aircraftIndex + 1) % aircraftCount);
      buildAircraftScroll(aircraftQueue[aircraftIndex]);
    }
    startScroll(cfg);
  }
}
