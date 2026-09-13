#include "adsb_client.h"

#include "../hardware/hub75_profile.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace adsb {
namespace {

constexpr size_t kMaxBodyBytes = 16384;
constexpr unsigned long kRefreshIntervalMs = 45UL * 1000UL;
constexpr unsigned long kRetryIntervalMs = 20UL * 1000UL;
constexpr float kDegToRad = 0.01745329252f;
constexpr uint32_t kWorkerStackBytes = 12288;
constexpr float kSpriteHalfPx = 2.0f;

struct ParsedAircraft {
  char hex[7];
  char callsign[9];
  char typeCode[5];
  float lat;
  float lon;
  float dstNm;
  int16_t gsKt;
  int16_t trackDeg;
  int32_t altFt;
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
  ParsedAircraft aircraft[kMaxTracks];
  uint8_t count;
  FetchStatus status;
  FetchRequest request;
};

enum class LaunchStatus : uint8_t {
  Started,
  Busy,
  Failed,
};

TrackedAircraft trackTable[kMaxTracks];
uint8_t trackUsed = 0;

portMUX_TYPE workerMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t workerHandle = nullptr;
FetchRequest workerRequest = {};
FetchResult pendingResult = {};
bool resultReady = false;
uint64_t currentGeneration = 0;
bool fetchNeeded = false;
bool fetchSucceeded = false;
unsigned long lastFetchMs = 0;
float lastLat = 0;
float lastLon = 0;
float lastRadius = 0;
uint8_t lastRadiusUnit = 0;

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

void projectOffsets(float obsLat, float obsLon, float lat, float lon,
                    float &eastNm, float &northNm) {
  northNm = (lat - obsLat) * 60.0f;
  eastNm = (lon - obsLon) * 60.0f * cosf(obsLat * kDegToRad);
}

float distanceNm(float eastNm, float northNm) {
  return sqrtf(eastNm * eastNm + northNm * northNm);
}

void insertNearest(FetchResult &result, const ParsedAircraft &candidate) {
  uint8_t insertAt = result.count;
  for (uint8_t i = 0; i < result.count; ++i) {
    if (candidate.dstNm < result.aircraft[i].dstNm) {
      insertAt = i;
      break;
    }
  }
  if (result.count < kMaxTracks) {
    for (uint8_t i = result.count; i > insertAt; --i) {
      result.aircraft[i] = result.aircraft[i - 1];
    }
    result.aircraft[insertAt] = candidate;
    ++result.count;
  } else if (insertAt < kMaxTracks) {
    for (uint8_t i = kMaxTracks - 1; i > insertAt; --i) {
      result.aircraft[i] = result.aircraft[i - 1];
    }
    result.aircraft[insertAt] = candidate;
  }
}

bool parseAircraftObject(JsonCursor &cursor, FetchResult &result) {
  if (!cursor.consume('{')) {
    return false;
  }

  ParsedAircraft aircraft = {};
  aircraft.dstNm = 9999.0f;
  aircraft.lat = NAN;
  aircraft.lon = NAN;
  bool onGround = false;
  bool hasTrack = false;
  String hex;
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

    if (key == "hex" || key == "flight" || key == "t") {
      String value;
      if (cursor.peek() == '"') {
        if (!cursor.parseString(value)) {
          return false;
        }
        value.trim();
        if (key == "hex") {
          hex = value;
        } else if (key == "flight") {
          flight = value;
        } else {
          typeCode = value;
        }
      } else if (!cursor.skipValue()) {
        return false;
      }
    } else if (key == "lat" || key == "lon" || key == "gs" || key == "dst" ||
               key == "track" || key == "true_heading" || key == "alt_baro") {
      if (key == "alt_baro" && cursor.peek() == '"') {
        String value;
        if (!cursor.parseString(value)) {
          return false;
        }
        onGround = value == "ground";
      } else if (cursor.peek() == '-' ||
                 (cursor.peek() >= '0' && cursor.peek() <= '9')) {
        double value;
        if (!cursor.parseNumber(value)) {
          return false;
        }
        if (key == "lat" && value >= -90.0 && value <= 90.0) {
          aircraft.lat = static_cast<float>(value);
        } else if (key == "lon" && value >= -180.0 && value <= 180.0) {
          aircraft.lon = static_cast<float>(value);
        } else if (key == "gs" && value >= 0.0 && value <= 2000.0) {
          aircraft.gsKt = static_cast<int16_t>(lround(value));
        } else if (key == "dst" && value >= 0.0 && value <= 10000.0) {
          aircraft.dstNm = static_cast<float>(value);
        } else if ((key == "track" || key == "true_heading") && value >= 0.0 &&
                   value <= 360.0) {
          if (!hasTrack || key == "track") {
            int rounded = static_cast<int>(lround(value));
            aircraft.trackDeg =
                static_cast<int16_t>(rounded == 360 ? 0 : rounded);
            hasTrack = true;
          }
        } else if (key == "alt_baro" && value >= -2000.0 &&
                   value <= 100000.0) {
          aircraft.altFt = static_cast<int32_t>(lround(value));
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

  if (onGround || hex.length() == 0 || !std::isfinite(aircraft.lat) ||
      !std::isfinite(aircraft.lon)) {
    return true;
  }
  copyCapped(aircraft.hex, sizeof(aircraft.hex), hex);
  copyCapped(aircraft.callsign, sizeof(aircraft.callsign), flight);
  copyCapped(aircraft.typeCode, sizeof(aircraft.typeCode), typeCode);
  insertNearest(result, aircraft);
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
  http.setUserAgent("ESP32-Tinker-FlightTracker");
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
  result.status = FetchStatus::HttpFailed;
  result.request = request;

  if (WiFi.status() != WL_CONNECTED) {
    result.status = FetchStatus::WifiUnavailable;
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
      result.status = parseAircraftList(body, result)
                          ? FetchStatus::Success
                          : FetchStatus::ParseFailed;
    }
  }

  portENTER_CRITICAL(&workerMux);
  pendingResult = result;
  resultReady = true;
  portEXIT_CRITICAL(&workerMux);
  vTaskSuspend(nullptr);
}

LaunchStatus launchFetch(const FlightTrackerSettings &settings,
                         uint64_t generation) {
  portENTER_CRITICAL(&workerMux);
  const bool busy = workerHandle != nullptr;
  portEXIT_CRITICAL(&workerMux);
  if (busy) {
    return LaunchStatus::Busy;
  }

  workerRequest = {settings.flightLat, settings.flightLon,
                   radiusNm(settings), generation};
  TaskHandle_t created = nullptr;
  const BaseType_t started =
      xTaskCreate(fetchWorker, "flight-fetch", kWorkerStackBytes, nullptr, 1,
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

void bumpGeneration() {
  ++currentGeneration;
  if (currentGeneration == 0) {
    ++currentGeneration;
  }
}

void rememberFetchConfig(const FlightTrackerSettings &settings) {
  lastLat = settings.flightLat;
  lastLon = settings.flightLon;
  lastRadius = settings.flightRadius;
  lastRadiusUnit = settings.flightRadiusUnit;
}

bool resultMatches(const FetchResult &result,
                   const FlightTrackerSettings &settings) {
  return result.request.generation == currentGeneration &&
         result.request.lat == settings.flightLat &&
         result.request.lon == settings.flightLon &&
         result.request.radiusNm == radiusNm(settings);
}

void applyOffsets(TrackedAircraft &track, const FlightTrackerSettings &settings) {
  projectOffsets(settings.flightLat, settings.flightLon, track.lat, track.lon,
                 track.eastNm, track.northNm);
}

#ifdef WOKWI_SIM
uint32_t locationSeed(const FlightTrackerSettings &settings) {
  const int32_t latMilli = lroundf(settings.flightLat * 1000.0f);
  const int32_t lonMilli = lroundf(settings.flightLon * 1000.0f);
  uint32_t seed = static_cast<uint32_t>(latMilli) * 73856093u ^
                  static_cast<uint32_t>(lonMilli) * 19349663u;
  return seed == 0 ? 1 : seed;
}

void loadFallbackTracks(const FlightTrackerSettings &settings) {
  const float range = radiusNm(settings);
  uint32_t seed = locationSeed(settings);
  trackUsed = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const float bearingDeg = static_cast<float>(seed % 360);
    seed = seed * 1664525u + 1013904223u;
    const float distNm = range * (0.25f + static_cast<float>(seed % 60) / 100.0f);
    seed = seed * 1664525u + 1013904223u;
    const int16_t trackDeg = static_cast<int16_t>(seed % 360);
    seed = seed * 1664525u + 1013904223u;
    const int16_t gsKt = static_cast<int16_t>(180 + seed % 280);

    const float bearingRad = bearingDeg * kDegToRad;
    TrackedAircraft &track = trackTable[trackUsed++];
    memset(&track, 0, sizeof(track));
    snprintf(track.hex, sizeof(track.hex), "sim%03u", i + 1);
    snprintf(track.callsign, sizeof(track.callsign), "SIM%u", i + 1);
    track.eastNm = distNm * sinf(bearingRad);
    track.northNm = distNm * cosf(bearingRad);
    track.gsKt = gsKt;
    track.trackDeg = trackDeg;
    track.lat = settings.flightLat + track.northNm / 60.0f;
    track.lon = settings.flightLon +
                track.eastNm / (60.0f * cosf(settings.flightLat * kDegToRad));
  }
  Serial.print(" flight_tracker fallback tracks=");
  Serial.print(trackUsed);
  Serial.print(" lat=");
  Serial.print(settings.flightLat, 5);
  Serial.print(" lon=");
  Serial.println(settings.flightLon, 5);
}
#endif

void applyFetch(const FetchResult &result,
                const FlightTrackerSettings &settings) {
  if (result.status != FetchStatus::Success) {
    Serial.print(" flight_tracker fetch status=");
    Serial.print(static_cast<int>(result.status));
    Serial.print(" lat=");
    Serial.print(result.request.lat, 5);
    Serial.print(" lon=");
    Serial.println(result.request.lon, 5);
#ifdef WOKWI_SIM
    if (trackUsed == 0) {
      loadFallbackTracks(settings);
    }
#endif
    return;
  }

  trackUsed = 0;
  for (uint8_t i = 0; i < result.count && trackUsed < kMaxTracks; ++i) {
    TrackedAircraft &track = trackTable[trackUsed++];
    memset(&track, 0, sizeof(track));
    memcpy(track.hex, result.aircraft[i].hex, sizeof(track.hex));
    memcpy(track.callsign, result.aircraft[i].callsign, sizeof(track.callsign));
    memcpy(track.typeCode, result.aircraft[i].typeCode, sizeof(track.typeCode));
    track.lat = result.aircraft[i].lat;
    track.lon = result.aircraft[i].lon;
    track.gsKt = result.aircraft[i].gsKt;
    track.trackDeg = result.aircraft[i].trackDeg;
    track.altFt = result.aircraft[i].altFt;
    applyOffsets(track, settings);
  }
  Serial.print(" flight_tracker tracks=");
  Serial.print(trackUsed);
  Serial.print(" lat=");
  Serial.print(settings.flightLat, 5);
  Serial.print(" lon=");
  Serial.println(settings.flightLon, 5);
}

float viewportRadiusPx() {
  const uint16_t width = flight_tracker_hardware::kDisplayWidth;
  const uint8_t height = flight_tracker_hardware::kDisplayHeight;
  return (width > height ? static_cast<float>(width)
                         : static_cast<float>(height)) *
         0.5f;
}

void deadReckon(unsigned long dtMs, const FlightTrackerSettings &settings) {
  if (dtMs == 0) {
    return;
  }
  const float dtSec = static_cast<float>(dtMs) * 0.001f;
  const float range = radiusNm(settings);
  const float maxDistNm =
      range * (1.0f + kSpriteHalfPx / viewportRadiusPx());

  uint8_t write = 0;
  for (uint8_t i = 0; i < trackUsed; ++i) {
    TrackedAircraft track = trackTable[i];
    const float nmPerSec = static_cast<float>(track.gsKt) / 3600.0f;
    const float trackRad = static_cast<float>(track.trackDeg) * kDegToRad;
    track.eastNm += sinf(trackRad) * nmPerSec * dtSec;
    track.northNm += cosf(trackRad) * nmPerSec * dtSec;
    if (distanceNm(track.eastNm, track.northNm) > maxDistNm) {
      continue;
    }
    trackTable[write++] = track;
  }
  trackUsed = write;
}

} // namespace

float radiusNm(const FlightTrackerSettings &settings) {
  constexpr float milesToNm = 0.868976f;
  constexpr float kmToNm = 0.539957f;
  return settings.flightRadius *
         (settings.flightRadiusUnit == FLIGHT_RADIUS_UNIT_KM ? kmToNm
                                                             : milesToNm);
}

void start(const FlightTrackerSettings &settings) {
  bumpGeneration();
  rememberFetchConfig(settings);
  lastFetchMs = millis();
  fetchSucceeded = false;
  fetchNeeded = true;
  trackUsed = 0;

  Serial.print(" flight_tracker search lat=");
  Serial.print(settings.flightLat, 5);
  Serial.print(" lon=");
  Serial.print(settings.flightLon, 5);
  Serial.print(" radiusNm=");
  Serial.println(radiusNm(settings));

  const LaunchStatus launch = launchFetch(settings, currentGeneration);
  if (launch == LaunchStatus::Started) {
    fetchNeeded = false;
  } else if (launch == LaunchStatus::Failed) {
    fetchNeeded = false;
    Serial.println(" flight_tracker fetch launch failed");
#ifdef WOKWI_SIM
    loadFallbackTracks(settings);
#endif
  }
}

void tick(const FlightTrackerSettings &settings, unsigned long dtMs) {
  deadReckon(dtMs, settings);

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
      applyFetch(result, settings);
      fetchSucceeded = result.status == FetchStatus::Success;
      lastFetchMs = millis();
      fetchNeeded = false;
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
        Serial.println(" flight_tracker fetch launch failed");
      }
    }
  }
}

const TrackedAircraft *tracks() { return trackTable; }

uint8_t trackCount() { return trackUsed; }

} // namespace adsb
