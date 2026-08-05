#include "real_weather.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>

namespace {
constexpr size_t SCROLL_BUFFER_SIZE = 256;
constexpr unsigned long REFRESH_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr unsigned long RETRY_INTERVAL_MS = 60UL * 1000UL;
constexpr uint8_t FORECAST_DAYS = 7;

char scrollTextBuffer[SCROLL_BUFFER_SIZE];
unsigned long lastFetchMs = 0;
bool fetchSucceeded = false;
String lastPostalCode;

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

int dayOfWeek(int year, int month, int day) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) {
    year -= 1;
  }
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) %
         7;
}

const char *weekdayName(int year, int month, int day) {
  static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return names[dayOfWeek(year, month, day)];
}

bool parseIsoDate(const String &value, int &year, int &month, int &day) {
  if (value.length() < 10 || value.charAt(4) != '-' || value.charAt(7) != '-') {
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
  while (valuePos < (int)json.length() &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    valuePos++;
  }
  out = json.substring(valuePos).toFloat();
  return true;
}

bool extractIntAfterKey(const String &json, const char *key, int &out,
                        int fromIndex = 0) {
  float value = 0;
  if (!extractNumberAfterKey(json, key, value, fromIndex)) {
    return false;
  }
  out = (int)lroundf(value);
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
  while (valuePos < (int)json.length() &&
         (json.charAt(valuePos) == ' ' || json.charAt(valuePos) == '\t')) {
    valuePos++;
  }
  if (valuePos >= (int)json.length() || json.charAt(valuePos) != '"') {
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
      {"Alabama", "AL"},
      {"Alaska", "AK"},
      {"Arizona", "AZ"},
      {"Arkansas", "AR"},
      {"California", "CA"},
      {"Colorado", "CO"},
      {"Connecticut", "CT"},
      {"Delaware", "DE"},
      {"District of Columbia", "DC"},
      {"Florida", "FL"},
      {"Georgia", "GA"},
      {"Hawaii", "HI"},
      {"Idaho", "ID"},
      {"Illinois", "IL"},
      {"Indiana", "IN"},
      {"Iowa", "IA"},
      {"Kansas", "KS"},
      {"Kentucky", "KY"},
      {"Louisiana", "LA"},
      {"Maine", "ME"},
      {"Maryland", "MD"},
      {"Massachusetts", "MA"},
      {"Michigan", "MI"},
      {"Minnesota", "MN"},
      {"Mississippi", "MS"},
      {"Missouri", "MO"},
      {"Montana", "MT"},
      {"Nebraska", "NE"},
      {"Nevada", "NV"},
      {"New Hampshire", "NH"},
      {"New Jersey", "NJ"},
      {"New Mexico", "NM"},
      {"New York", "NY"},
      {"North Carolina", "NC"},
      {"North Dakota", "ND"},
      {"Ohio", "OH"},
      {"Oklahoma", "OK"},
      {"Oregon", "OR"},
      {"Pennsylvania", "PA"},
      {"Rhode Island", "RI"},
      {"South Carolina", "SC"},
      {"South Dakota", "SD"},
      {"Tennessee", "TN"},
      {"Texas", "TX"},
      {"Utah", "UT"},
      {"Vermont", "VT"},
      {"Virginia", "VA"},
      {"Washington", "WA"},
      {"West Virginia", "WV"},
      {"Wisconsin", "WI"},
      {"Wyoming", "WY"},
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
  if (town.length() > 0) {
    const char *abbr = usStateAbbreviation(admin1);
    if (abbr != nullptr) {
      return town + ", " + abbr;
    }
    return town;
  }
  return postalCode;
}

int findArrayAfterKey(const String &json, const char *key) {
  String needle = String("\"") + key + "\":";
  int keyPos = json.indexOf(needle);
  if (keyPos < 0) {
    return -1;
  }
  int bracket = json.indexOf('[', keyPos + needle.length());
  return bracket;
}

bool parseNumberArray(const String &json, const char *key, float *values,
                      uint8_t maxCount, uint8_t &count) {
  count = 0;
  int bracket = findArrayAfterKey(json, key);
  if (bracket < 0) {
    return false;
  }
  int pos = bracket + 1;
  while (pos < (int)json.length() && count < maxCount) {
    while (pos < (int)json.length() &&
           (json.charAt(pos) == ' ' || json.charAt(pos) == ',' ||
            json.charAt(pos) == '\n' || json.charAt(pos) == '\r')) {
      pos++;
    }
    if (pos >= (int)json.length() || json.charAt(pos) == ']') {
      break;
    }
    values[count++] = json.substring(pos).toFloat();
    while (pos < (int)json.length() && json.charAt(pos) != ',' &&
           json.charAt(pos) != ']') {
      pos++;
    }
  }
  return count > 0;
}

bool parseStringArray(const String &json, const char *key, String *values,
                      uint8_t maxCount, uint8_t &count) {
  count = 0;
  int bracket = findArrayAfterKey(json, key);
  if (bracket < 0) {
    return false;
  }
  int pos = bracket + 1;
  while (pos < (int)json.length() && count < maxCount) {
    int quote = json.indexOf('"', pos);
    if (quote < 0) {
      break;
    }
    int endQuote = json.indexOf('"', quote + 1);
    if (endQuote < 0) {
      break;
    }
    values[count++] = json.substring(quote + 1, endQuote);
    pos = endQuote + 1;
    int close = json.indexOf(']', pos);
    int comma = json.indexOf(',', pos);
    if (close >= 0 && (comma < 0 || close < comma)) {
      break;
    }
    if (comma < 0) {
      break;
    }
    pos = comma + 1;
  }
  return count > 0;
}

bool httpGet(const String &url, String &body) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(10000);
  http.setReuse(false);
  if (!http.begin(url)) {
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  body = http.getString();
  http.end();
  return body.length() > 0;
}

bool geocodePostalCode(const String &postalCode, float &latitude,
                       float &longitude, String &town, String &admin1) {
  String url = "http://geocoding-api.open-meteo.com/v1/search?name=";
  url += postalCode;
  url += "&count=1&countryCode=US";

  String body;
  if (!httpGet(url, body)) {
    return false;
  }
  int resultsPos = body.indexOf("\"results\"");
  if (resultsPos < 0) {
    return false;
  }
  if (!extractNumberAfterKey(body, "latitude", latitude, resultsPos) ||
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
  url += "&current=temperature_2m,weather_code";
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
  url += "&temperature_unit=fahrenheit&forecast_days=7&timezone=auto";
  return httpGet(url, body);
}

void setScrollText(const char *text) {
  strncpy(scrollTextBuffer, text, SCROLL_BUFFER_SIZE - 1);
  scrollTextBuffer[SCROLL_BUFFER_SIZE - 1] = '\0';
}

void startScroll(const ProgramConfig &cfg) {
  Display.displayClear();
  Display.setIntensity(cfg.brightness > 15 ? 15 : cfg.brightness);
  Display.setTextAlignment(PA_LEFT);
  unsigned int speed =
      cfg.scrollSpeedMs > 0 ? cfg.scrollSpeedMs : 75U;
  Display.displayScroll(scrollTextBuffer, PA_LEFT, PA_SCROLL_LEFT, speed);
}

bool buildWeatherScroll(const String &locationLabel, const String &forecastJson,
                        char *out, size_t outSize) {
  int currentSection = forecastJson.indexOf("\"current\":");
  if (currentSection < 0) {
    return false;
  }

  float currentTemp = 0;
  int currentCode = 0;
  if (!extractNumberAfterKey(forecastJson, "temperature_2m", currentTemp,
                             currentSection) ||
      !extractIntAfterKey(forecastJson, "weather_code", currentCode,
                          currentSection)) {
    return false;
  }

  int dailySection = forecastJson.indexOf("\"daily\":");
  if (dailySection < 0) {
    return false;
  }

  String dates[FORECAST_DAYS];
  float dailyCodes[FORECAST_DAYS];
  float dailyMax[FORECAST_DAYS];
  float dailyMin[FORECAST_DAYS];
  uint8_t dateCount = 0;
  uint8_t codeCount = 0;
  uint8_t maxCount = 0;
  uint8_t minCount = 0;

  String dailyJson = forecastJson.substring(dailySection);
  if (!parseStringArray(dailyJson, "time", dates, FORECAST_DAYS, dateCount) ||
      !parseNumberArray(dailyJson, "weather_code", dailyCodes, FORECAST_DAYS,
                        codeCount) ||
      !parseNumberArray(dailyJson, "temperature_2m_max", dailyMax, FORECAST_DAYS,
                        maxCount) ||
      !parseNumberArray(dailyJson, "temperature_2m_min", dailyMin, FORECAST_DAYS,
                        minCount)) {
    return false;
  }

  uint8_t days = dateCount;
  if (codeCount < days) {
    days = codeCount;
  }
  if (maxCount < days) {
    days = maxCount;
  }
  if (minCount < days) {
    days = minCount;
  }
  if (days == 0) {
    return false;
  }

  String message = locationLabel;
  message += " Now ";
  message += String((int)lroundf(currentTemp));
  message += "F ";
  message += weatherLabel(currentCode);

  for (uint8_t i = 0; i < days; i++) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseIsoDate(dates[i], year, month, day)) {
      continue;
    }
    message += " | ";
    message += weekdayName(year, month, day);
    message += " ";
    message += String((int)lroundf(dailyMax[i]));
    message += "/";
    message += String((int)lroundf(dailyMin[i]));
    message += " ";
    message += weatherLabel((int)lroundf(dailyCodes[i]));
  }

  if (message.length() >= outSize) {
    message = message.substring(0, outSize - 1);
  }
  strncpy(out, message.c_str(), outSize - 1);
  out[outSize - 1] = '\0';
  return true;
}

bool refreshWeather(const ProgramConfig &cfg) {
  String postalCode = cfg.weatherPostalCode;
  postalCode.trim();
  if (postalCode.length() == 0) {
    setScrollText("Set ZIP in setup");
    fetchSucceeded = false;
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setScrollText("Weather unavailable");
    fetchSucceeded = false;
    return false;
  }

  float latitude = 0;
  float longitude = 0;
  String town;
  String admin1;
  if (!geocodePostalCode(postalCode, latitude, longitude, town, admin1)) {
    setScrollText("Weather unavailable");
    fetchSucceeded = false;
    return false;
  }

  String forecastJson;
  if (!fetchForecast(latitude, longitude, forecastJson)) {
    setScrollText("Weather unavailable");
    fetchSucceeded = false;
    return false;
  }

  String locationLabel = buildLocationLabel(town, admin1, postalCode);
  if (!buildWeatherScroll(locationLabel, forecastJson, scrollTextBuffer,
                          SCROLL_BUFFER_SIZE)) {
    setScrollText("Weather unavailable");
    fetchSucceeded = false;
    return false;
  }

  fetchSucceeded = true;
  return true;
}
} // namespace

void realWeatherStart(const ProgramConfig &cfg) {
  lastPostalCode = cfg.weatherPostalCode;
  lastPostalCode.trim();
  lastFetchMs = 0;
  setScrollText("Loading weather...");
  startScroll(cfg);

  if (refreshWeather(cfg)) {
    lastFetchMs = millis();
  } else {
    lastFetchMs = millis();
  }
  startScroll(cfg);
}

void realWeatherTick(const ProgramConfig &cfg) {
  String postalCode = cfg.weatherPostalCode;
  postalCode.trim();
  unsigned long now = millis();
  unsigned long interval = fetchSucceeded ? REFRESH_INTERVAL_MS : RETRY_INTERVAL_MS;

  if (postalCode != lastPostalCode || now - lastFetchMs >= interval) {
    lastPostalCode = postalCode;
    if (refreshWeather(cfg)) {
      lastFetchMs = now;
      startScroll(cfg);
    } else {
      lastFetchMs = now;
      startScroll(cfg);
    }
  }

  if (Display.displayAnimate()) {
    Display.displayReset();
  }
}
