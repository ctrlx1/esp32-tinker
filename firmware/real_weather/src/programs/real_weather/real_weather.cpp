#include "real_weather.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

constexpr unsigned long kRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr unsigned long kRetryIntervalMs = 60UL * 1000UL;
constexpr unsigned long kHttpTimeoutMs = 10000UL;
constexpr unsigned long kWorkerResultDeadlineMs = 30UL * 1000UL;
constexpr size_t kGeocodeBodyLimit = 4096;
constexpr size_t kForecastBodyLimit = 12288;
constexpr size_t kPostalCapacity = REAL_WEATHER_MAX_POSTAL_CODE_LENGTH + 1;
constexpr size_t kMessageCapacity = 384;
constexpr uint8_t kForecastDays = 7;
constexpr uint32_t kWorkerStackDepth = 8192;

struct FetchRequest {
  char postalCode[kPostalCapacity];
  uint32_t generation;
};

struct FetchResult {
  char postalCode[kPostalCapacity];
  char message[kMessageCapacity];
  uint32_t generation;
  bool succeeded;
};

QueueHandle_t requestQueue = nullptr;
QueueHandle_t resultQueue = nullptr;
TaskHandle_t workerTask = nullptr;
bool requestPending = false;
bool fetchSucceeded = false;
unsigned long lastAttemptMs = 0;
unsigned long requestStartedMs = 0;
char cachedPostalCode[kPostalCapacity] = "";
uint32_t currentGeneration = 0;

const char *weatherLabel(int code) {
  if (code == 0) {
    return "Clear";
  }
  if (code <= 3) {
    return "Cloudy";
  }
  if (code <= 48) {
    return "Fog";
  }
  if (code <= 67 || (code >= 80 && code <= 82)) {
    return "Rain";
  }
  if (code <= 77 || (code >= 85 && code <= 86)) {
    return "Snow";
  }
  if (code >= 95) {
    return "Storm";
  }
  return "Mixed";
}

const char *windDirectionLabel(int degrees) {
  int normalized = degrees % 360;
  if (normalized < 0) {
    normalized += 360;
  }
  static const char *labels[] = {"N", "NE", "E", "SE",
                                 "S", "SW", "W", "NW"};
  return labels[((normalized + 22) % 360) / 45];
}

int dayOfWeek(int year, int month, int day) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) {
    --year;
  }
  return (year + year / 4 - year / 100 + year / 400 +
          offsets[month - 1] + day) %
         7;
}

const char *weekdayName(int year, int month, int day) {
  static const char *names[] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
  return names[dayOfWeek(year, month, day)];
}

bool parseIsoDate(const String &value, int &year, int &month, int &day) {
  if (value.length() != 10 || value.charAt(4) != '-' ||
      value.charAt(7) != '-') {
    return false;
  }
  year = value.substring(0, 4).toInt();
  month = value.substring(5, 7).toInt();
  day = value.substring(8, 10).toInt();
  return year > 0 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool extractNumberAfterKey(const String &json, const char *key, float &out,
                           int fromIndex = 0) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle, fromIndex);
  if (keyPos < 0) {
    return false;
  }
  int valuePos = keyPos + needle.length();
  while (valuePos < static_cast<int>(json.length()) &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    ++valuePos;
  }
  if (valuePos >= static_cast<int>(json.length())) {
    return false;
  }
  char *end = nullptr;
  out = strtof(json.c_str() + valuePos, &end);
  return end != json.c_str() + valuePos && std::isfinite(out);
}

bool extractIntAfterKey(const String &json, const char *key, int &out,
                        int fromIndex = 0) {
  float value = 0;
  if (!extractNumberAfterKey(json, key, value, fromIndex)) {
    return false;
  }
  out = static_cast<int>(lroundf(value));
  return true;
}

bool extractStringAfterKey(const String &json, const char *key, String &out,
                           int fromIndex = 0) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle, fromIndex);
  if (keyPos < 0) {
    return false;
  }
  int valuePos = keyPos + needle.length();
  while (valuePos < static_cast<int>(json.length()) &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    ++valuePos;
  }
  if (valuePos >= static_cast<int>(json.length()) ||
      json.charAt(valuePos) != '"') {
    return false;
  }
  int endQuote = json.indexOf('"', valuePos + 1);
  if (endQuote < 0) {
    return false;
  }
  out = json.substring(valuePos + 1, endQuote);
  return true;
}

const char *usStateAbbreviation(const String &admin1) {
  struct StateEntry {
    const char *name;
    const char *abbr;
  };
  static const StateEntry states[] = {
      {"Alabama", "AL"},       {"Alaska", "AK"},
      {"Arizona", "AZ"},       {"Arkansas", "AR"},
      {"California", "CA"},    {"Colorado", "CO"},
      {"Connecticut", "CT"},   {"Delaware", "DE"},
      {"District of Columbia", "DC"},
      {"Florida", "FL"},       {"Georgia", "GA"},
      {"Hawaii", "HI"},        {"Idaho", "ID"},
      {"Illinois", "IL"},      {"Indiana", "IN"},
      {"Iowa", "IA"},          {"Kansas", "KS"},
      {"Kentucky", "KY"},      {"Louisiana", "LA"},
      {"Maine", "ME"},         {"Maryland", "MD"},
      {"Massachusetts", "MA"}, {"Michigan", "MI"},
      {"Minnesota", "MN"},     {"Mississippi", "MS"},
      {"Missouri", "MO"},      {"Montana", "MT"},
      {"Nebraska", "NE"},      {"Nevada", "NV"},
      {"New Hampshire", "NH"}, {"New Jersey", "NJ"},
      {"New Mexico", "NM"},    {"New York", "NY"},
      {"North Carolina", "NC"},{"North Dakota", "ND"},
      {"Ohio", "OH"},          {"Oklahoma", "OK"},
      {"Oregon", "OR"},        {"Pennsylvania", "PA"},
      {"Rhode Island", "RI"},  {"South Carolina", "SC"},
      {"South Dakota", "SD"},  {"Tennessee", "TN"},
      {"Texas", "TX"},         {"Utah", "UT"},
      {"Vermont", "VT"},       {"Virginia", "VA"},
      {"Washington", "WA"},    {"West Virginia", "WV"},
      {"Wisconsin", "WI"},     {"Wyoming", "WY"},
  };
  for (const StateEntry &entry : states) {
    if (admin1.equalsIgnoreCase(entry.name)) {
      return entry.abbr;
    }
  }
  return nullptr;
}

String buildLocationLabel(const String &town, const String &admin1,
                          const String &postalCode) {
  if (town.length() == 0) {
    return postalCode;
  }
  const char *abbr = usStateAbbreviation(admin1);
  return abbr ? town + ", " + abbr : town;
}

int findArrayAfterKey(const String &json, const char *key) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle);
  return keyPos < 0 ? -1 : json.indexOf('[', keyPos + needle.length());
}

bool parseNumberArray(const String &json, const char *key, float *values,
                      uint8_t maxCount, uint8_t &count) {
  count = 0;
  int pos = findArrayAfterKey(json, key);
  if (pos < 0) {
    return false;
  }
  ++pos;
  while (pos < static_cast<int>(json.length()) && count < maxCount) {
    while (pos < static_cast<int>(json.length()) &&
           (json.charAt(pos) == ' ' || json.charAt(pos) == ',' ||
            json.charAt(pos) == '\n' || json.charAt(pos) == '\r')) {
      ++pos;
    }
    if (pos >= static_cast<int>(json.length()) || json.charAt(pos) == ']') {
      break;
    }
    char *end = nullptr;
    float value = strtof(json.c_str() + pos, &end);
    if (end == json.c_str() + pos || !std::isfinite(value)) {
      return false;
    }
    values[count++] = value;
    pos = static_cast<int>(end - json.c_str());
  }
  return count > 0;
}

bool parseStringArray(const String &json, const char *key, String *values,
                      uint8_t maxCount, uint8_t &count) {
  count = 0;
  int pos = findArrayAfterKey(json, key);
  if (pos < 0) {
    return false;
  }
  ++pos;
  while (pos < static_cast<int>(json.length()) && count < maxCount) {
    while (pos < static_cast<int>(json.length()) &&
           (json.charAt(pos) == ' ' || json.charAt(pos) == ',' ||
            json.charAt(pos) == '\n' || json.charAt(pos) == '\r')) {
      ++pos;
    }
    if (pos >= static_cast<int>(json.length()) || json.charAt(pos) == ']') {
      break;
    }
    if (json.charAt(pos) != '"') {
      return false;
    }
    int endQuote = json.indexOf('"', pos + 1);
    if (endQuote < 0) {
      return false;
    }
    values[count++] = json.substring(pos + 1, endQuote);
    pos = endQuote + 1;
  }
  return count > 0;
}

class BoundedStringStream : public Stream {
public:
  BoundedStringStream(String &body, size_t capacity)
      : body_(body), capacity_(capacity) {
    body_ = "";
    body_.reserve(capacity_);
  }

  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (body_.length() + size > capacity_) {
      overflowed_ = true;
      return 0;
    }
    return body_.concat(reinterpret_cast<const char *>(buffer),
                        static_cast<unsigned int>(size))
               ? size
               : 0;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  bool overflowed() const { return overflowed_; }

private:
  String &body_;
  size_t capacity_;
  bool overflowed_ = false;
};

bool readBoundedResponse(HTTPClient &http, String &body, size_t maxBytes) {
  int contentLength = http.getSize();
  if (contentLength > static_cast<int>(maxBytes)) {
    return false;
  }
  BoundedStringStream sink(body, maxBytes);
  int written = http.writeToStream(&sink);
  return written > 0 && !sink.overflowed() && body.length() > 0;
}

bool httpGetBounded(const String &url, String &body, size_t maxBytes) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(kHttpTimeoutMs);
  http.setReuse(false);
  if (!http.begin(url)) {
    return false;
  }
  int code = http.GET();
  bool ok = code == HTTP_CODE_OK && readBoundedResponse(http, body, maxBytes);
  http.end();
  return ok;
}

bool geocodePostalCode(const String &postalCode, float &latitude,
                       float &longitude, String &town, String &admin1) {
  String url = "http://geocoding-api.open-meteo.com/v1/search?name=";
  url += postalCode.substring(0, 5);
  url += "&count=1&countryCode=US";
  String body;
  if (!httpGetBounded(url, body, kGeocodeBodyLimit)) {
    return false;
  }
  int resultsPos = body.indexOf("\"results\"");
  if (resultsPos < 0 ||
      !extractNumberAfterKey(body, "latitude", latitude, resultsPos) ||
      !extractNumberAfterKey(body, "longitude", longitude, resultsPos)) {
    return false;
  }
  town = "";
  admin1 = "";
  extractStringAfterKey(body, "name", town, resultsPos);
  extractStringAfterKey(body, "admin1", admin1, resultsPos);
  return true;
}

bool fetchForecast(float latitude, float longitude, String &body) {
  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(latitude, 4);
  url += "&longitude=";
  url += String(longitude, 4);
  url += "&current=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m";
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
  url += "&temperature_unit=fahrenheit&wind_speed_unit=mph";
  url += "&forecast_days=7&timezone=auto";
  return httpGetBounded(url, body, kForecastBodyLimit);
}

bool appendFormat(char *out, size_t capacity, size_t &used,
                  const char *format, ...) {
  if (used >= capacity) {
    return false;
  }
  va_list args;
  va_start(args, format);
  int written = vsnprintf(out + used, capacity - used, format, args);
  va_end(args);
  if (written < 0) {
    return false;
  }
  if (static_cast<size_t>(written) >= capacity - used) {
    used = capacity - 1;
    out[used] = '\0';
    return false;
  }
  used += static_cast<size_t>(written);
  return true;
}

bool buildWeatherScroll(const String &location, const String &forecastJson,
                        char *out, size_t outSize) {
  int currentSection = forecastJson.indexOf("\"current\":");
  int dailySection = forecastJson.indexOf("\"daily\":");
  if (currentSection < 0 || dailySection < 0) {
    return false;
  }

  float currentTemp = 0;
  int currentCode = 0;
  float windSpeedMph = 0;
  int windDirectionDeg = 0;
  if (!extractNumberAfterKey(forecastJson, "temperature_2m", currentTemp,
                             currentSection) ||
      !extractIntAfterKey(forecastJson, "weather_code", currentCode,
                          currentSection) ||
      !extractNumberAfterKey(forecastJson, "wind_speed_10m", windSpeedMph,
                             currentSection) ||
      !extractIntAfterKey(forecastJson, "wind_direction_10m", windDirectionDeg,
                          currentSection)) {
    return false;
  }

  String dailyJson = forecastJson.substring(dailySection);
  String dates[kForecastDays];
  float dailyCodes[kForecastDays];
  float dailyMax[kForecastDays];
  float dailyMin[kForecastDays];
  uint8_t dateCount = 0;
  uint8_t codeCount = 0;
  uint8_t maxCount = 0;
  uint8_t minCount = 0;
  if (!parseStringArray(dailyJson, "time", dates, kForecastDays, dateCount) ||
      !parseNumberArray(dailyJson, "weather_code", dailyCodes, kForecastDays,
                        codeCount) ||
      !parseNumberArray(dailyJson, "temperature_2m_max", dailyMax,
                        kForecastDays, maxCount) ||
      !parseNumberArray(dailyJson, "temperature_2m_min", dailyMin,
                        kForecastDays, minCount)) {
    return false;
  }
  uint8_t days = min(min(dateCount, codeCount), min(maxCount, minCount));
  if (days != kForecastDays) {
    return false;
  }

  size_t used = 0;
  out[0] = '\0';
  if (!appendFormat(out, outSize, used, "%s Now %dF %s %dmph %s",
                    location.c_str(),
                    static_cast<int>(lroundf(currentTemp)),
                    windDirectionLabel(windDirectionDeg),
                    static_cast<int>(lroundf(windSpeedMph)),
                    weatherLabel(currentCode))) {
    return false;
  }
  for (uint8_t i = 0; i < days; ++i) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseIsoDate(dates[i], year, month, day)) {
      return false;
    }
    if (!appendFormat(out, outSize, used, " | %s %d/%d %s",
                      weekdayName(year, month, day),
                      static_cast<int>(lroundf(dailyMax[i])),
                      static_cast<int>(lroundf(dailyMin[i])),
                      weatherLabel(
                          static_cast<int>(lroundf(dailyCodes[i]))))) {
      return false;
    }
  }
  return used > 0;
}

void performFetch(const FetchRequest &request, FetchResult &result) {
  memset(&result, 0, sizeof(result));
  strncpy(result.postalCode, request.postalCode, sizeof(result.postalCode) - 1);
  result.generation = request.generation;
  strncpy(result.message, "Weather unavailable",
          sizeof(result.message) - 1);

  float latitude = 0;
  float longitude = 0;
  String town;
  String admin1;
  String postalCode(request.postalCode);
  if (!geocodePostalCode(postalCode, latitude, longitude, town, admin1)) {
    return;
  }
  String forecastJson;
  if (!fetchForecast(latitude, longitude, forecastJson)) {
    return;
  }
  String location = buildLocationLabel(town, admin1, postalCode);
  char completedMessage[kMessageCapacity] = "";
  if (buildWeatherScroll(location, forecastJson, completedMessage,
                         sizeof(completedMessage))) {
    strncpy(result.message, completedMessage, sizeof(result.message) - 1);
    result.message[sizeof(result.message) - 1] = '\0';
    result.succeeded = true;
  }
}

void weatherWorker(void *) {
  FetchRequest request{};
  for (;;) {
    if (xQueueReceive(requestQueue, &request, portMAX_DELAY) == pdTRUE) {
      FetchResult result{};
      performFetch(request, result);
      xQueueOverwrite(resultQueue, &result);
    }
  }
}

bool ensureWorker() {
  if (workerTask != nullptr) {
    return true;
  }
  requestQueue = xQueueCreate(1, sizeof(FetchRequest));
  resultQueue = xQueueCreate(1, sizeof(FetchResult));
  if (requestQueue == nullptr || resultQueue == nullptr) {
    if (requestQueue != nullptr) {
      vQueueDelete(requestQueue);
    }
    if (resultQueue != nullptr) {
      vQueueDelete(resultQueue);
    }
    requestQueue = nullptr;
    resultQueue = nullptr;
    return false;
  }
  if (xTaskCreate(weatherWorker, "real-weather", kWorkerStackDepth, nullptr, 1,
                  &workerTask) != pdPASS) {
    vQueueDelete(requestQueue);
    vQueueDelete(resultQueue);
    requestQueue = nullptr;
    resultQueue = nullptr;
    workerTask = nullptr;
    return false;
  }
  return true;
}

void startScroll(const ProgramConfig &cfg, const char *text) {
  tinker::RuntimeContext &runtime = programRuntimeContext();
  runtime.copyText(text);
  runtime.setBrightness(cfg.brightness);
  runtime.clearText();
  runtime.startTextScroll(runtime.textBuffer(), tinker::TextAlignment::Left,
                          cfg.scrollSpeedMs);
}

bool startFetch() {
  if (cachedPostalCode[0] == '\0' || requestPending || !ensureWorker()) {
    return false;
  }
  FetchRequest request{};
  strncpy(request.postalCode, cachedPostalCode,
          sizeof(request.postalCode) - 1);
  request.generation = currentGeneration;
  if (xQueueOverwrite(requestQueue, &request) != pdTRUE) {
    return false;
  }
  requestPending = true;
  requestStartedMs = millis();
  lastAttemptMs = requestStartedMs;
  return true;
}

void applyCompletedFetch(const ProgramConfig &cfg) {
  if (resultQueue == nullptr) {
    return;
  }
  FetchResult result{};
  if (xQueueReceive(resultQueue, &result, 0) != pdTRUE) {
    return;
  }
  if (result.generation != currentGeneration) {
    return;
  }
  requestPending = false;
  lastAttemptMs = millis();
  fetchSucceeded = result.succeeded;
  startScroll(cfg, result.message);
}

void expireStuckRequest(const ProgramConfig &cfg, unsigned long now) {
  if (!requestPending ||
      now - requestStartedMs < kWorkerResultDeadlineMs) {
    return;
  }
  requestPending = false;
  ++currentGeneration;
  fetchSucceeded = false;
  lastAttemptMs = now;
  startScroll(cfg, "Weather unavailable");
}

} // namespace

void realWeatherStart(const ProgramConfig &cfg) {
  ++currentGeneration;
  String postalCode = cfg.weatherPostalCode;
  postalCode.trim();
  postalCode.toCharArray(cachedPostalCode, sizeof(cachedPostalCode));
  requestPending = false;
  fetchSucceeded = false;
  lastAttemptMs = millis();

  if (cachedPostalCode[0] == '\0') {
    startScroll(cfg, "Set ZIP in setup");
    return;
  }
  startScroll(cfg, "Loading weather...");
  if (!startFetch()) {
    startScroll(cfg, "Weather unavailable");
  }
}

void realWeatherTick(const ProgramConfig &cfg) {
  applyCompletedFetch(cfg);
  unsigned long now = millis();
  expireStuckRequest(cfg, now);
  if (cachedPostalCode[0] != '\0' && !requestPending) {
    unsigned long interval =
        fetchSucceeded ? kRefreshIntervalMs : kRetryIntervalMs;
    if (now - lastAttemptMs >= interval) {
      startFetch();
    }
  }

  tinker::RuntimeContext &runtime = programRuntimeContext();
  if (runtime.animateText()) {
    runtime.resetTextAnimation();
  }
}
