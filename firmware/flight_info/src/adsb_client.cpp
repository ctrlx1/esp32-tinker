#include "adsb_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <pgmspace.h>

namespace adsb {
namespace {

constexpr size_t kMaxBodyBytes = 16384;
constexpr unsigned long kRefreshIntervalMs = 45UL * 1000UL;
constexpr unsigned long kRetryIntervalMs = 20UL * 1000UL;
constexpr float kMiToNm = 0.868976f;
constexpr float kKmToNm = 0.539957f;
constexpr uint32_t kWorkerStackBytes = 12288;

struct FetchRequest {
  float lat;
  float lon;
  float radiusNm;
  uint64_t generation;
};

struct FetchResult {
  Aircraft aircraft[kMaxAircraft];
  uint8_t count;
  Status status;
  FetchRequest request;
};

enum class LaunchStatus : uint8_t {
  Started,
  Busy,
  Failed,
};

Aircraft aircraftQueue[kMaxAircraft];
uint8_t aircraftCount = 0;
Status fetchStatus = Status::Loading;
unsigned long lastFetchMs = 0;
bool fetchSucceeded = false;
float lastLat = 0;
float lastLon = 0;
float lastRadius = 0;
uint8_t lastRadiusUnit = 0;

portMUX_TYPE workerMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t workerHandle = nullptr;
FetchRequest workerRequest = {};
FetchResult pendingResult = {};
bool resultReady = false;
uint64_t currentGeneration = 0;
bool fetchNeeded = false;

float radiusToNm(float radius, uint8_t unit) {
  return radius * (unit == FLIGHT_RADIUS_UNIT_KM ? kKmToNm : kMiToNm);
}

float radiusNm(const FlightInfoSettings &settings) {
  return radiusToNm(settings.flightRadius, settings.flightRadiusUnit);
}

void bumpGeneration() {
  ++currentGeneration;
  if (currentGeneration == 0) {
    ++currentGeneration;
  }
}

void rememberFetchConfig(const FlightInfoSettings &settings) {
  lastLat = settings.flightLat;
  lastLon = settings.flightLon;
  lastRadius = settings.flightRadius;
  lastRadiusUnit = settings.flightRadiusUnit;
}

bool resultMatches(const FetchResult &result,
                   const FlightInfoSettings &settings) {
  return result.request.generation == currentGeneration &&
         result.request.lat == settings.flightLat &&
         result.request.lon == settings.flightLon &&
         result.request.radiusNm == radiusNm(settings);
}

void copyCapped(char *dest, size_t destSize, const String &src) {
  if (destSize == 0) {
    return;
  }
  const size_t count =
      src.length() < destSize ? src.length() : destSize - 1;
  memcpy(dest, src.c_str(), count);
  dest[count] = '\0';
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
    const char delimiter = position_ < length_ ? data_[position_] : '\0';
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

void insertSorted(FetchResult &result, const Aircraft &candidate) {
  uint8_t insertAt = result.count;
  for (uint8_t i = 0; i < result.count; ++i) {
    if (candidate.dstNm < result.aircraft[i].dstNm) {
      insertAt = i;
      break;
    }
  }
  if (result.count < kMaxAircraft) {
    for (uint8_t i = result.count; i > insertAt; --i) {
      result.aircraft[i] = result.aircraft[i - 1];
    }
    result.aircraft[insertAt] = candidate;
    ++result.count;
  } else if (insertAt < kMaxAircraft) {
    for (uint8_t i = kMaxAircraft - 1; i > insertAt; --i) {
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

  if (!onGround && hasAltitude && flight.length() > 0) {
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
  http.setUserAgent("ESP32-Tinker-FlightInfo");
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
  if (stream == nullptr || contentLength > static_cast<int>(kMaxBodyBytes)) {
    http.end();
    return false;
  }
  if (!body.reserve(kMaxBodyBytes)) {
    http.end();
    return false;
  }

  unsigned long idleStart = millis();
  while (http.connected() && body.length() < kMaxBodyBytes) {
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
    while (available-- > 0 && body.length() < kMaxBodyBytes) {
      const int value = stream->read();
      if (value < 0) {
        break;
      }
      body += static_cast<char>(value);
    }
  }
  const bool hitUnknownLengthCap =
      contentLength < 0 && body.length() >= kMaxBodyBytes;
  const bool complete =
      body.length() > 0 && !hitUnknownLengthCap &&
      (contentLength < 0 || body.length() == static_cast<size_t>(contentLength));
  http.end();
  return complete;
}

void fetchWorker(void *) {
  const FetchRequest request = workerRequest;
  FetchResult result = {};
  result.status = Status::HttpFailed;
  result.request = request;

  if (WiFi.status() != WL_CONNECTED) {
    result.status = Status::WifiUnavailable;
  } else {
    char url[128];
    snprintf(url, sizeof(url),
             "http://api.adsb.lol/v2/lat/%.5f/lon/%.5f/dist/%.2f", request.lat,
             request.lon, request.radiusNm);
    String body;
    bool fetched = httpGetCapped(url, body);
    if (!fetched) {
      snprintf(url, sizeof(url),
               "http://api.adsb.lol/v2/closest/%.5f/%.5f/%.2f", request.lat,
               request.lon, request.radiusNm);
      fetched = httpGetCapped(url, body);
    }
    if (fetched) {
      result.status = parseAircraftList(body, result) ? Status::Success
                                                      : Status::ParseFailed;
    }
  }

  portENTER_CRITICAL(&workerMux);
  pendingResult = result;
  resultReady = true;
  portEXIT_CRITICAL(&workerMux);

  vTaskSuspend(nullptr);
}

LaunchStatus launchFetch(const FlightInfoSettings &settings,
                         uint64_t generation) {
  portENTER_CRITICAL(&workerMux);
  const bool busy = workerHandle != nullptr;
  portEXIT_CRITICAL(&workerMux);
  if (busy) {
    return LaunchStatus::Busy;
  }

  workerRequest = {settings.flightLat, settings.flightLon, radiusNm(settings),
                   generation};
  TaskHandle_t created = nullptr;
  const BaseType_t started =
      xTaskCreate(fetchWorker, "flight-info", kWorkerStackBytes, nullptr, 1,
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

#ifdef WOKWI_SIM
void loadFallbackAircraft(const FlightInfoSettings &settings) {
  struct Demo {
    const char *callsign;
    const char *typeCode;
    int32_t altFt;
    int16_t gsKt;
    int16_t trackDeg;
    int16_t vertRateFpm;
    float dstFraction;
  };
  constexpr Demo kDemo[] = {
      {"JBU123", "A320", 35000, 478, 270, 1400, 0.28f},
      {"AAL456", "B738", 28000, 420, 90, -800, 0.42f},
      {"UAL789", "B77W", 38000, 510, 180, 0, 0.55f},
      {"DAL321", "A321", 12000, 310, 45, 600, 0.67f},
      {"SWA555", "B737", 8000, 280, 315, -200, 0.81f},
      {"FFT210", "A20N", 24000, 390, 135, 900, 0.36f},
      {"NKS884", "A321", 16000, 340, 225, -500, 0.48f},
      {"ASA172", "B739", 33000, 460, 300, 200, 0.61f},
      {"FDX401", "B763", 36000, 490, 15, -100, 0.74f},
      {"UPS980", "B748", 39000, 520, 75, 0, 0.88f},
  };
  constexpr uint8_t kDemoCount = sizeof(kDemo) / sizeof(kDemo[0]);

  const float range = radiusNm(settings);
  aircraftCount = 0;
  for (uint8_t i = 0; i < kDemoCount && i < kMaxAircraft; ++i) {
    Aircraft &item = aircraftQueue[aircraftCount++];
    memset(&item, 0, sizeof(item));
    strncpy(item.callsign, kDemo[i].callsign, sizeof(item.callsign) - 1);
    strncpy(item.typeCode, kDemo[i].typeCode, sizeof(item.typeCode) - 1);
    item.altFt = kDemo[i].altFt;
    item.gsKt = kDemo[i].gsKt;
    item.trackDeg = kDemo[i].trackDeg;
    item.vertRateFpm = kDemo[i].vertRateFpm;
    item.hasVertRate = true;
    item.dstNm = range * kDemo[i].dstFraction;
  }
  fetchStatus = Status::Success;
  fetchSucceeded = true;
  Serial.print(" flight_info fallback aircraft=");
  Serial.print(aircraftCount);
  Serial.print(" lat=");
  Serial.print(settings.flightLat, 5);
  Serial.print(" lon=");
  Serial.println(settings.flightLon, 5);
}
#endif

void applyFetch(const FetchResult &result, const FlightInfoSettings &settings) {
  fetchStatus = result.status;
  if (result.status != Status::Success) {
    Serial.print(" flight_info fetch status=");
    Serial.print(static_cast<int>(result.status));
    Serial.print(" lat=");
    Serial.print(result.request.lat, 5);
    Serial.print(" lon=");
    Serial.println(result.request.lon, 5);
#ifdef WOKWI_SIM
    if (aircraftCount == 0) {
      loadFallbackAircraft(settings);
    }
#endif
    return;
  }

  aircraftCount = result.count;
  memcpy(aircraftQueue, result.aircraft, sizeof(aircraftQueue));
  Serial.print(" flight_info aircraft=");
  Serial.print(aircraftCount);
  Serial.print(" lat=");
  Serial.print(settings.flightLat, 5);
  Serial.print(" lon=");
  Serial.println(settings.flightLon, 5);
}

} // namespace

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

void start(const FlightInfoSettings &settings) {
  bumpGeneration();
  rememberFetchConfig(settings);
  lastFetchMs = millis();
  aircraftCount = 0;
  fetchSucceeded = false;
  fetchNeeded = true;
  fetchStatus = Status::Loading;

  const LaunchStatus launch = launchFetch(settings, currentGeneration);
  if (launch == LaunchStatus::Started) {
    fetchNeeded = false;
  } else if (launch == LaunchStatus::Failed) {
    fetchNeeded = false;
    fetchStatus = Status::HttpFailed;
#ifdef WOKWI_SIM
    loadFallbackAircraft(settings);
#endif
  }
}

void tick(const FlightInfoSettings &settings) {
  const bool locationChanged =
      settings.flightLat != lastLat || settings.flightLon != lastLon ||
      settings.flightRadius != lastRadius ||
      settings.flightRadiusUnit != lastRadiusUnit;
  if (locationChanged) {
    bumpGeneration();
    rememberFetchConfig(settings);
    fetchNeeded = true;
  }

  FetchResult result;
  if (consumeResult(result)) {
    if (resultMatches(result, settings)) {
      fetchSucceeded = result.status == Status::Success;
      lastFetchMs = millis();
      fetchNeeded = false;
      applyFetch(result, settings);
    } else {
      fetchNeeded = true;
    }
  }

  const unsigned long now = millis();
  const unsigned long interval =
      fetchSucceeded ? kRefreshIntervalMs : kRetryIntervalMs;
  if (now - lastFetchMs >= interval) {
    fetchNeeded = true;
  }
  if (fetchNeeded) {
    const LaunchStatus launch = launchFetch(settings, currentGeneration);
    if (launch != LaunchStatus::Busy) {
      lastFetchMs = now;
      fetchNeeded = false;
      if (launch == LaunchStatus::Failed) {
        fetchSucceeded = false;
        fetchStatus = Status::HttpFailed;
#ifdef WOKWI_SIM
        if (aircraftCount == 0) {
          loadFallbackAircraft(settings);
        }
#endif
      }
    }
  }
}

const Aircraft *items() { return aircraftQueue; }

uint8_t count() { return aircraftCount; }

Status status() { return fetchStatus; }

} // namespace adsb
