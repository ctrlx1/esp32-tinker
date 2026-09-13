#include "flight_watch.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <pgmspace.h>

namespace {
constexpr uint8_t MAX_AIRCRAFT = 3;
constexpr size_t MAX_BODY_BYTES = 8192;
constexpr unsigned long REFRESH_INTERVAL_MS = 45UL * 1000UL;
constexpr unsigned long RETRY_INTERVAL_MS = 20UL * 1000UL;
constexpr int VERT_RATE_LEVEL_FPM = 100;
constexpr float MI_TO_NM = 0.868976f;
constexpr float KM_TO_NM = 0.539957f;
constexpr uint32_t WORKER_STACK_BYTES = 12288;

struct Aircraft {
  char callsign[9];
  char typeCode[5];
  int32_t altFt;
  int16_t gsKt;
  int16_t trackDeg;
  int16_t vertRateFpm;
  bool hasVertRate;
  float dstNm;
};

enum class FetchStatus : uint8_t {
  Success,
  WifiUnavailable,
  HttpFailed,
  ParseFailed,
};

struct FetchRequest {
  float lat;
  float lon;
  float radiusNm;
  uint64_t generation;
};

struct FetchResult {
  Aircraft aircraft[MAX_AIRCRAFT];
  uint8_t count;
  FetchStatus status;
  FetchRequest request;
};

enum class LaunchStatus : uint8_t {
  Started,
  Busy,
  Failed,
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
uint8_t lastSpeedUnit = 0;

portMUX_TYPE workerMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t workerHandle = nullptr;
FetchRequest workerRequest = {};
FetchResult pendingResult = {};
bool resultReady = false;
uint64_t currentGeneration = 0;
bool fetchNeeded = false;

bool airlineNameFromCallsign(const char *callsign, char *out, size_t outSize) {
  struct AirlineEntry {
    char icao[4];
    char name[12];
  };
  static const AirlineEntry airlines[] PROGMEM = {
      {"AAL", "American"},   {"UAL", "United"},    {"DAL", "Delta"},
      {"SWA", "Southwest"},  {"JBU", "JetBlue"},   {"ASA", "Alaska"},
      {"FFT", "Frontier"},   {"NKS", "Spirit"},    {"HAL", "Hawaiian"},
      {"BAW", "British"},    {"AFR", "AirFrance"}, {"DLH", "Lufthansa"},
      {"UAE", "Emirates"},   {"ACA", "AirCanada"}, {"SKW", "SkyWest"},
      {"ENY", "Envoy"},      {"RPA", "RepubAir"},  {"EDV", "Endeavor"},
      {"FDX", "FedEx"},      {"UPS", "UPS"},        {"KLM", "KLM"},
      {"IBE", "Iberia"},     {"RYR", "Ryanair"},   {"VIR", "Virgin"},
      {"QFA", "Qantas"},
  };

  if (outSize == 0 || callsign == nullptr || strlen(callsign) < 3) {
    return false;
  }
  char prefix[4] = {callsign[0], callsign[1], callsign[2], '\0'};
  for (uint8_t i = 0; i < 3; ++i) {
    if (prefix[i] >= 'a' && prefix[i] <= 'z') {
      prefix[i] = static_cast<char>(prefix[i] - 'a' + 'A');
    }
  }

  AirlineEntry entry;
  for (size_t i = 0; i < sizeof(airlines) / sizeof(airlines[0]); ++i) {
    memcpy_P(&entry, &airlines[i], sizeof(entry));
    if (strcmp(prefix, entry.icao) == 0) {
      strncpy(out, entry.name, outSize - 1);
      out[outSize - 1] = '\0';
      return true;
    }
  }
  return false;
}

class JsonCursor {
public:
  explicit JsonCursor(const String &json)
      : data_(json.c_str()), length_(json.length()) {}

  void skipWhitespace() {
    while (position_ < length_ &&
           (data_[position_] == ' ' || data_[position_] == '\t' ||
            data_[position_] == '\r' || data_[position_] == '\n')) {
      ++position_;
    }
  }

  bool consume(char expected) {
    skipWhitespace();
    if (position_ >= length_ || data_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  char peek() {
    skipWhitespace();
    return position_ < length_ ? data_[position_] : '\0';
  }

  bool atEnd() {
    skipWhitespace();
    return position_ == length_;
  }

  bool parseString(String &out) {
    skipWhitespace();
    if (position_ >= length_ || data_[position_++] != '"') {
      return false;
    }
    out = "";
    while (position_ < length_) {
      const char value = data_[position_++];
      if (value == '"') {
        return true;
      }
      if (static_cast<uint8_t>(value) < 0x20) {
        return false;
      }
      if (value != '\\') {
        out += value;
        continue;
      }
      if (position_ >= length_) {
        return false;
      }
      const char escaped = data_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        out += escaped;
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u':
        for (uint8_t i = 0; i < 4; ++i) {
          if (position_ >= length_ || !isHex(data_[position_++])) {
            return false;
          }
        }
        out += '?';
        break;
      default:
        return false;
      }
    }
    return false;
  }

  bool parseNumber(double &out) {
    skipWhitespace();
    const size_t start = position_;
    if (position_ < length_ && data_[position_] == '-') {
      ++position_;
    }
    if (position_ >= length_) {
      return false;
    }
    if (data_[position_] == '0') {
      ++position_;
      if (position_ < length_ && isDigit(data_[position_])) {
        return false;
      }
    } else if (data_[position_] >= '1' && data_[position_] <= '9') {
      while (position_ < length_ && isDigit(data_[position_])) {
        ++position_;
      }
    } else {
      return false;
    }
    if (position_ < length_ && data_[position_] == '.') {
      ++position_;
      const size_t fractionStart = position_;
      while (position_ < length_ && isDigit(data_[position_])) {
        ++position_;
      }
      if (position_ == fractionStart) {
        return false;
      }
    }
    if (position_ < length_ &&
        (data_[position_] == 'e' || data_[position_] == 'E')) {
      ++position_;
      if (position_ < length_ &&
          (data_[position_] == '+' || data_[position_] == '-')) {
        ++position_;
      }
      const size_t exponentStart = position_;
      while (position_ < length_ && isDigit(data_[position_])) {
        ++position_;
      }
      if (position_ == exponentStart) {
        return false;
      }
    }
    const char delimiter =
        position_ < length_ ? data_[position_] : '\0';
    if (delimiter != '\0' && delimiter != ',' && delimiter != '}' &&
        delimiter != ']' && delimiter != ' ' && delimiter != '\t' &&
        delimiter != '\r' && delimiter != '\n') {
      return false;
    }
    String token;
    if (!token.reserve(position_ - start + 1)) {
      return false;
    }
    for (size_t i = start; i < position_; ++i) {
      token += data_[i];
    }
    char *end = nullptr;
    out = strtod(token.c_str(), &end);
    return end != token.c_str() && *end == '\0' && std::isfinite(out);
  }

  bool skipValue(uint8_t depth = 0) {
    if (depth > 16) {
      return false;
    }
    skipWhitespace();
    if (position_ >= length_) {
      return false;
    }
    if (data_[position_] == '"') {
      String ignored;
      return parseString(ignored);
    }
    if (data_[position_] == '{') {
      ++position_;
      skipWhitespace();
      if (position_ < length_ && data_[position_] == '}') {
        ++position_;
        return true;
      }
      while (true) {
        String key;
        if (!parseString(key) || !consume(':') || !skipValue(depth + 1)) {
          return false;
        }
        if (consume('}')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (data_[position_] == '[') {
      ++position_;
      skipWhitespace();
      if (position_ < length_ && data_[position_] == ']') {
        ++position_;
        return true;
      }
      while (true) {
        if (!skipValue(depth + 1)) {
          return false;
        }
        if (consume(']')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (matchLiteral("true") || matchLiteral("false") ||
        matchLiteral("null")) {
      return true;
    }
    double ignored;
    return parseNumber(ignored);
  }

private:
  static bool isDigit(char value) { return value >= '0' && value <= '9'; }
  static bool isHex(char value) {
    return isDigit(value) || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
  }

  bool matchLiteral(const char *literal) {
    const size_t count = strlen(literal);
    if (position_ + count > length_ ||
        strncmp(data_ + position_, literal, count) != 0) {
      return false;
    }
    const size_t end = position_ + count;
    const char delimiter = end < length_ ? data_[end] : '\0';
    if (delimiter != '\0' && delimiter != ',' && delimiter != '}' &&
        delimiter != ']' && delimiter != ' ' && delimiter != '\t' &&
        delimiter != '\r' && delimiter != '\n') {
      return false;
    }
    position_ = end;
    return true;
  }

  const char *data_;
  size_t length_;
  size_t position_ = 0;
};

void copyCapped(char *dest, size_t destSize, const String &src) {
  if (destSize == 0) {
    return;
  }
  const size_t count =
      src.length() < destSize ? src.length() : destSize - 1;
  memcpy(dest, src.c_str(), count);
  dest[count] = '\0';
}

void insertSorted(FetchResult &result, const Aircraft &candidate) {
  uint8_t insertAt = result.count;
  for (uint8_t i = 0; i < result.count; ++i) {
    if (candidate.dstNm < result.aircraft[i].dstNm) {
      insertAt = i;
      break;
    }
  }
  if (result.count < MAX_AIRCRAFT) {
    for (uint8_t i = result.count; i > insertAt; --i) {
      result.aircraft[i] = result.aircraft[i - 1];
    }
    result.aircraft[insertAt] = candidate;
    ++result.count;
  } else if (insertAt < MAX_AIRCRAFT) {
    for (uint8_t i = MAX_AIRCRAFT - 1; i > insertAt; --i) {
      result.aircraft[i] = result.aircraft[i - 1];
    }
    result.aircraft[insertAt] = candidate;
  }
}

bool parseAircraftObject(JsonCursor &cursor, FetchResult &result) {
  if (!cursor.consume('{')) {
    return false;
  }

  Aircraft aircraft = {};
  aircraft.dstNm = 9999.0f;
  bool hasAltitude = false;
  bool onGround = false;
  bool hasTrack = false;
  String flight;
  String typeCode;

  if (cursor.consume('}')) {
    return true;
  }
  while (true) {
    String key;
    if (!cursor.parseString(key) || !cursor.consume(':')) {
      return false;
    }

    if (key == "flight" || key == "t") {
      String value;
      if (cursor.peek() == '"') {
        if (!cursor.parseString(value)) {
          return false;
        }
        if (key == "flight") {
          flight = value;
          flight.trim();
        } else {
          typeCode = value;
          typeCode.trim();
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (key == "alt_baro") {
      if (cursor.peek() == '"') {
        String value;
        if (!cursor.parseString(value)) {
          return false;
        }
        onGround = value == "ground";
      } else {
        double value;
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if (value >= -2000.0 && value <= 100000.0) {
          aircraft.altFt = static_cast<int32_t>(lround(value));
          hasAltitude = true;
        }
      }
    } else if (key == "gs") {
      double value;
      if (cursor.peek() == '-' ||
          (cursor.peek() >= '0' && cursor.peek() <= '9')) {
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if (value >= 0.0 && value <= 2000.0) {
          aircraft.gsKt = static_cast<int16_t>(lround(value));
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (key == "track" || key == "true_heading") {
      double value;
      if (cursor.peek() == '-' ||
          (cursor.peek() >= '0' && cursor.peek() <= '9')) {
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if ((!hasTrack || key == "track") && value >= 0.0 && value <= 360.0) {
          int rounded = static_cast<int>(lround(value));
          aircraft.trackDeg = static_cast<int16_t>(rounded == 360 ? 0 : rounded);
          hasTrack = true;
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (key == "baro_rate" || key == "geom_rate") {
      double value;
      if (cursor.peek() == '-' ||
          (cursor.peek() >= '0' && cursor.peek() <= '9')) {
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if (!aircraft.hasVertRate && value >= -20000.0 && value <= 20000.0) {
          aircraft.vertRateFpm = static_cast<int16_t>(lround(value));
          aircraft.hasVertRate = true;
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (key == "dst") {
      double value;
      if (cursor.peek() == '-' ||
          (cursor.peek() >= '0' && cursor.peek() <= '9')) {
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if (value >= 0.0 && value <= 10000.0) {
          aircraft.dstNm = static_cast<float>(value);
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (!cursor.skipValue()) {
      return false;
    }

    if (cursor.consume('}')) {
      break;
    }
    if (!cursor.consume(',')) {
      return false;
    }
  }

  if (!onGround && hasAltitude) {
    copyCapped(aircraft.callsign, sizeof(aircraft.callsign), flight);
    copyCapped(aircraft.typeCode, sizeof(aircraft.typeCode), typeCode);
    insertSorted(result, aircraft);
  }
  return true;
}

bool parseAircraftArray(JsonCursor &cursor, FetchResult &result) {
  if (!cursor.consume('[')) {
    return false;
  }
  if (cursor.consume(']')) {
    return true;
  }
  while (true) {
    if (!parseAircraftObject(cursor, result)) {
      return false;
    }
    if (cursor.consume(']')) {
      return true;
    }
    if (!cursor.consume(',')) {
      return false;
    }
  }
}

bool parseAircraftList(const String &json, FetchResult &result) {
  result.count = 0;
  JsonCursor cursor(json);
  if (!cursor.consume('{')) {
    return false;
  }

  bool foundAircraft = false;
  if (cursor.consume('}')) {
    return false;
  }
  while (true) {
    String key;
    if (!cursor.parseString(key) || !cursor.consume(':')) {
      return false;
    }
    if (key == "ac") {
      if (foundAircraft || !parseAircraftArray(cursor, result)) {
        return false;
      }
      foundAircraft = true;
    } else if (!cursor.skipValue()) {
      return false;
    }
    if (cursor.consume('}')) {
      break;
    }
    if (!cursor.consume(',')) {
      return false;
    }
  }
  return foundAircraft && cursor.atEnd();
}

bool httpGetCapped(const String &url, String &body) {
  body = "";
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(15000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("ESP32-Tinker-FlightWatch");
  if (!http.begin(url)) {
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  const int contentLength = http.getSize();
  if (stream == nullptr || contentLength > static_cast<int>(MAX_BODY_BYTES)) {
    http.end();
    return false;
  }
  if (!body.reserve(MAX_BODY_BYTES)) {
    http.end();
    return false;
  }

  unsigned long idleStart = millis();
  while (http.connected() && body.length() < MAX_BODY_BYTES) {
    size_t available = stream->available();
    if (available == 0) {
      if (contentLength >= 0 &&
          body.length() >= static_cast<size_t>(contentLength)) {
        break;
      }
      if (millis() - idleStart > 4000) {
        break;
      }
      vTaskDelay(1);
      continue;
    }
    idleStart = millis();
    while (available-- > 0 && body.length() < MAX_BODY_BYTES) {
      const int value = stream->read();
      if (value < 0) {
        break;
      }
      body += static_cast<char>(value);
    }
  }
  const bool hitUnknownLengthCap =
      contentLength < 0 && body.length() >= MAX_BODY_BYTES;
  const bool complete =
      body.length() > 0 && !hitUnknownLengthCap &&
      (contentLength < 0 || body.length() == static_cast<size_t>(contentLength));
  http.end();
  return complete;
}

void fetchWorker(void *) {
  const FetchRequest request = workerRequest;
  FetchResult result = {};
  result.status = FetchStatus::HttpFailed;
  result.request = request;

  if (WiFi.status() != WL_CONNECTED) {
    result.status = FetchStatus::WifiUnavailable;
  } else {
    char url[128];
    snprintf(url, sizeof(url),
             "http://api.adsb.lol/v2/lat/%.5f/lon/%.5f/dist/%.2f",
             request.lat, request.lon, request.radiusNm);
    String body;
    bool fetched = httpGetCapped(url, body);
    if (!fetched) {
      snprintf(url, sizeof(url),
               "http://api.adsb.lol/v2/closest/%.5f/%.5f/%.2f", request.lat,
               request.lon, request.radiusNm);
      fetched = httpGetCapped(url, body);
    }
    if (fetched) {
      result.status = parseAircraftList(body, result)
                          ? FetchStatus::Success
                          : FetchStatus::ParseFailed;
    }
  }

  portENTER_CRITICAL(&workerMux);
  pendingResult = result;
  resultReady = true;
  portEXIT_CRITICAL(&workerMux);

  // Main-loop ownership performs deletion after consuming the result. Keeping
  // the handle non-null until then prevents a second worker from being started.
  vTaskSuspend(nullptr);
}

float radiusToNm(float radius, uint8_t unit) {
  return radius *
         (unit == FLIGHT_RADIUS_UNIT_KM ? KM_TO_NM : MI_TO_NM);
}

LaunchStatus launchFetch(const ProgramConfig &cfg, uint64_t generation) {
  portENTER_CRITICAL(&workerMux);
  const bool busy = workerHandle != nullptr;
  portEXIT_CRITICAL(&workerMux);
  if (busy) {
    return LaunchStatus::Busy;
  }

  workerRequest = {cfg.flightLat, cfg.flightLon,
                   radiusToNm(cfg.flightRadius, cfg.flightRadiusUnit),
                   generation};
  TaskHandle_t created = nullptr;
  const BaseType_t started =
      xTaskCreate(fetchWorker, "flight-fetch", WORKER_STACK_BYTES, nullptr, 1,
                  &created);
  if (started != pdPASS) {
    return LaunchStatus::Failed;
  }
  portENTER_CRITICAL(&workerMux);
  workerHandle = created;
  portEXIT_CRITICAL(&workerMux);
  return LaunchStatus::Started;
}

bool consumeResult(FetchResult &result) {
  TaskHandle_t completedWorker = nullptr;
  portENTER_CRITICAL(&workerMux);
  if (resultReady) {
    result = pendingResult;
    resultReady = false;
    completedWorker = workerHandle;
    workerHandle = nullptr;
  }
  portEXIT_CRITICAL(&workerMux);
  if (completedWorker == nullptr) {
    return false;
  }
  vTaskDelete(completedWorker);
  return true;
}

void setScrollText(const char *text) {
  programRuntimeContext().copyText(text);
}

void startScroll(const ProgramConfig &cfg) {
  tinker::RuntimeContext &runtime = programRuntimeContext();
  runtime.clearText();
  runtime.setBrightness(cfg.brightness);
  runtime.startTextScroll(runtime.textBuffer(), tinker::TextAlignment::Left,
                          cfg.scrollSpeedMs);
}

void buildAircraftScroll(const Aircraft &aircraft, uint8_t speedUnit) {
  String message;
  char airline[12];
  if (airlineNameFromCallsign(aircraft.callsign, airline, sizeof(airline))) {
    message += airline;
    message += " ";
  }
  message += aircraft.callsign[0] ? aircraft.callsign : "Aircraft";
  if (aircraft.typeCode[0]) {
    message += " ";
    message += aircraft.typeCode;
  }
  message += " ";
  message += String(aircraft.altFt);
  message += "ft ";

  int speed = aircraft.gsKt;
  const char *suffix = "kt";
  if (speedUnit == FLIGHT_SPEED_UNIT_MPH) {
    speed = static_cast<int>(lroundf(aircraft.gsKt * 1.15078f));
    suffix = "mph";
  } else if (speedUnit == FLIGHT_SPEED_UNIT_KPH) {
    speed = static_cast<int>(lroundf(aircraft.gsKt * 1.852f));
    suffix = "kph";
  }
  message += String(speed);
  message += suffix;
  message += " ";
  char track[8];
  snprintf(track, sizeof(track), "%03d", aircraft.trackDeg);
  message += track;
  message += "deg";
  if (aircraft.hasVertRate) {
    message += " ";
    message += String(aircraft.vertRateFpm);
    message += "fpm ";
    message += aircraft.vertRateFpm >= VERT_RATE_LEVEL_FPM
                   ? "CLIMB"
                   : (aircraft.vertRateFpm <= -VERT_RATE_LEVEL_FPM ? "DESC"
                                                                   : "LEVEL");
  }

  const size_t capacity = programRuntimeContext().textBufferSize();
  if (capacity > 0 && message.length() >= capacity) {
    message.remove(capacity - 1);
  }
  setScrollText(message.c_str());
}

void showCurrentOrStatus(const ProgramConfig &cfg, FetchStatus status) {
  if (aircraftCount > 0) {
    if (aircraftIndex >= aircraftCount) {
      aircraftIndex = 0;
    }
    buildAircraftScroll(aircraftQueue[aircraftIndex], cfg.flightSpeedUnit);
  } else if (status == FetchStatus::Success) {
    setScrollText("No aircraft nearby");
  } else if (status == FetchStatus::WifiUnavailable) {
    setScrollText("Flight unavailable");
  } else if (status == FetchStatus::ParseFailed) {
    setScrollText("Flight parse failed");
  } else {
    setScrollText("Flight fetch failed");
  }
  startScroll(cfg);
}

void rememberFetchConfig(const ProgramConfig &cfg) {
  lastLat = cfg.flightLat;
  lastLon = cfg.flightLon;
  lastRadius = cfg.flightRadius;
  lastRadiusUnit = cfg.flightRadiusUnit;
}

bool resultMatchesCurrentRequest(const FetchResult &result,
                                 const ProgramConfig &cfg) {
  return result.request.generation == currentGeneration &&
         result.request.lat == cfg.flightLat &&
         result.request.lon == cfg.flightLon &&
         result.request.radiusNm ==
             radiusToNm(cfg.flightRadius, cfg.flightRadiusUnit);
}
} // namespace

void flightWatchStart(const ProgramConfig &cfg) {
  ++currentGeneration;
  if (currentGeneration == 0) {
    ++currentGeneration;
  }
  rememberFetchConfig(cfg);
  lastSpeedUnit = cfg.flightSpeedUnit;
  lastFetchMs = millis();
  aircraftCount = 0;
  aircraftIndex = 0;
  fetchSucceeded = false;
  fetchNeeded = true;
  setScrollText("Loading flights...");
  startScroll(cfg);
  const LaunchStatus launch = launchFetch(cfg, currentGeneration);
  if (launch == LaunchStatus::Started) {
    fetchNeeded = false;
  } else if (launch == LaunchStatus::Failed) {
    fetchNeeded = false;
    showCurrentOrStatus(cfg, FetchStatus::HttpFailed);
  }
}

void flightWatchTick(const ProgramConfig &cfg) {
  const bool locationChanged =
      cfg.flightLat != lastLat || cfg.flightLon != lastLon ||
      cfg.flightRadius != lastRadius ||
      cfg.flightRadiusUnit != lastRadiusUnit;
  if (locationChanged) {
    ++currentGeneration;
    if (currentGeneration == 0) {
      ++currentGeneration;
    }
    rememberFetchConfig(cfg);
    fetchNeeded = true;
  }

  FetchResult result;
  if (consumeResult(result)) {
    if (resultMatchesCurrentRequest(result, cfg)) {
      aircraftCount = result.count;
      aircraftIndex = 0;
      memcpy(aircraftQueue, result.aircraft, sizeof(aircraftQueue));
      fetchSucceeded = result.status == FetchStatus::Success;
      lastFetchMs = millis();
      fetchNeeded = false;
      showCurrentOrStatus(cfg, result.status);
    } else {
      fetchNeeded = true;
    }
  }

  const unsigned long now = millis();
  const unsigned long interval =
      fetchSucceeded ? REFRESH_INTERVAL_MS : RETRY_INTERVAL_MS;
  if (now - lastFetchMs >= interval) {
    fetchNeeded = true;
  }
  if (fetchNeeded) {
    const LaunchStatus launch = launchFetch(cfg, currentGeneration);
    if (launch != LaunchStatus::Busy) {
      lastFetchMs = now;
      fetchNeeded = false;
      if (launch == LaunchStatus::Failed) {
        fetchSucceeded = false;
        showCurrentOrStatus(cfg, FetchStatus::HttpFailed);
      }
    }
  }

  if (cfg.flightSpeedUnit != lastSpeedUnit) {
    lastSpeedUnit = cfg.flightSpeedUnit;
    showCurrentOrStatus(cfg, fetchSucceeded ? FetchStatus::Success
                                            : FetchStatus::HttpFailed);
    return;
  }

  if (programRuntimeContext().animateText()) {
    if (aircraftCount > 0) {
      aircraftIndex = static_cast<uint8_t>((aircraftIndex + 1) % aircraftCount);
      buildAircraftScroll(aircraftQueue[aircraftIndex], cfg.flightSpeedUnit);
    }
    startScroll(cfg);
  }
}
