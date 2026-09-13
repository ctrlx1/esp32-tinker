#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#ifdef WOKWI_SIM
#include <esp_phy_init.h>
#endif

#include "portal_request.h"
#include "project_definition.h"
#include "runtime_context.h"

namespace tinker {

template <typename DisplayAdapter, typename Project> class TinkerApp {
public:
  explicit TinkerApp(const typename DisplayAdapter::Config &displayConfig)
      : display_(displayConfig), runtime_(display_.runtimeContext()),
        project_(runtime_), server_(80) {}

  void setup() {
    Serial.begin(9600);

    display_.begin();
    runtime_.showBootVersion(project_.definition().version);
    loadSettings();
    runtime_.setBrightness(project_.displayBrightness());

#ifdef WOKWI_SIM
    if (!startWokwiStation()) {
      startConfigPortal();
    }
#else
    if (savedSsid_.length() > 0) {
      if (connectToSavedWiFi()) {
        registerRoutes();
        server_.begin();
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
    if (configMode_) {
      dnsServer_.processNextRequest();
      server_.handleClient();
      runtime_.tickSetup();
      return;
    }

    server_.handleClient();
    project_.tickPrograms();
  }

private:
  static uint16_t dnsPort() { return 53; }
  static unsigned long wifiTimeoutMs() { return 15000UL; }
  static unsigned long wokwiSetupTimeoutMs() { return 8000UL; }
  static const char *wokwiGuestSsid() { return "Wokwi-GUEST"; }

  static bool runningInWokwi() {
#ifdef WOKWI_SIM
    return true;
#else
    return false;
#endif
  }

  void loadSettings() {
    SettingsSchema schema = project_.definition().settings;
    Preferences preferences;
    preferences.begin(schema.nvsNamespace, !schema.isVersioned());
    if (schema.isVersioned()) {
      uint16_t storedVersion = preferences.getUShort(schema.versionKey, 0);
      if (storedVersion < schema.currentVersion) {
        if (project_.migrateSettings(preferences, storedVersion)) {
          preferences.putUShort(schema.versionKey, schema.currentVersion);
        } else {
          settingsWritable_ = false;
          Serial.println(
              "Settings migration failed; preserving stored settings.");
        }
      } else if (storedVersion > schema.currentVersion) {
        settingsWritable_ = false;
        Serial.println(
            "Settings are from a newer firmware; saving is disabled.");
      }
      preferences.end();
      preferences.begin(schema.nvsNamespace, true);
    }
    savedSsid_ = preferences.getString("ssid", "");
    savedPass_ = preferences.getString("pass", "");
    Serial.print("Config loaded: ssid=");
    Serial.print(savedSsid_.length() ? savedSsid_ : "(empty)");
    project_.loadSettings(preferences);
    preferences.end();
  }

  void saveSettings(const String &ssid, const String &password) {
    SettingsSchema schema = project_.definition().settings;
    Preferences preferences;
    preferences.begin(schema.nvsNamespace, false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    project_.saveSettings(preferences);
    if (schema.isVersioned()) {
      preferences.putUShort(schema.versionKey, schema.currentVersion);
    }
    preferences.end();
    savedSsid_ = ssid;
    savedPass_ = password;
  }

  String setupApSsid() {
    uint8_t mac[6];
    char suffix[5];
    WiFi.macAddress(mac);
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String(project_.definition().setupApSsidPrefix) + suffix;
  }

  String buildPage() {
    String ip =
        configMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    return project_.buildPortalPage(ip, savedSsid_);
  }

  void handleRoot() { server_.send(200, "text/html", buildPage()); }

  void handleSave() {
    if (!settingsWritable_) {
      server_.send(409, "text/plain",
                   "Settings cannot be saved by this firmware version.");
      return;
    }
    PortalRequest request(server_);
    String error;
    if (!project_.applyPortalRequest(request, error)) {
      server_.send(400, "text/plain", error);
      return;
    }

    String newSsid = request.arg("ssid");
    newSsid.trim();
    if (newSsid.length() == 0) {
      if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == wokwiGuestSsid()) {
        newSsid = runningInWokwi() ? "" : String(wokwiGuestSsid());
      } else {
        newSsid = savedSsid_;
      }
    }

    String newPass = request.arg("pass");
    if (newPass.length() == 0) {
      newPass = savedPass_;
    }
    if (runningInWokwi() && newSsid == wokwiGuestSsid()) {
      newSsid = "";
      newPass = "";
    }

    saveSettings(newSsid, newPass);
    server_.send(200, "text/plain", "Saved! Rebooting now...");
    delay(1500);
    ESP.restart();
  }

  static bool isDecimalComponent(const String &value, int start, int end) {
    if (end <= start) {
      return false;
    }
    if (value.charAt(start) == '0' && end - start > 1) {
      return false;
    }
    for (int index = start; index < end; ++index) {
      const char digit = value.charAt(index);
      if (digit < '0' || digit > '9') {
        return false;
      }
    }
    return true;
  }

  static bool isStrictSemver(const String &value) {
    const int first = value.indexOf('.');
    const int second = value.indexOf('.', first + 1);
    if (first <= 0 || second <= first + 1 ||
        value.indexOf('.', second + 1) >= 0) {
      return false;
    }
    return isDecimalComponent(value, 0, first) &&
           isDecimalComponent(value, first + 1, second) &&
           isDecimalComponent(value, second + 1, value.length());
  }

  static bool isProjectIdStyle(const String &value) {
    if (value.length() == 0) {
      return false;
    }
    bool previousSeparator = true;
    for (unsigned index = 0; index < value.length(); ++index) {
      const char character = value.charAt(index);
      const bool alnum = (character >= 'a' && character <= 'z') ||
                         (character >= '0' && character <= '9');
      const bool separator = character == '-' || character == '_';
      if (alnum) {
        previousSeparator = false;
        continue;
      }
      if (!separator || previousSeparator) {
        return false;
      }
      previousSeparator = true;
    }
    return !previousSeparator;
  }

  static String uploadBasename(const String &filename) {
    String base = filename;
    const int slash = base.lastIndexOf('/');
    if (slash >= 0) {
      base = base.substring(slash + 1);
    }
    const int backslash = base.lastIndexOf('\\');
    if (backslash >= 0) {
      base = base.substring(backslash + 1);
    }
    return base;
  }

  String expectedFirmwareFilename() const {
    return String("firmware.bin");
  }

  bool otaFilenameAllowed(const String &filename) const {
    const String base = uploadBasename(filename);
    if (!base.endsWith(".bin")) {
      return true;
    }
    const String stem = base.substring(0, base.length() - 4);
    const int underscore = stem.lastIndexOf('_');
    if (underscore <= 0) {
      return true;
    }
    const String prefix = stem.substring(0, underscore);
    const String version = stem.substring(underscore + 1);
    if (!isProjectIdStyle(prefix) || !isStrictSemver(version)) {
      return true;
    }

    const ProjectDefinition definition = project_.definition();
    if (prefix == "firmware") {
      return true;
    }
    return prefix == definition.id;
  }

  void handleUpdate() {
    HTTPUpload &upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
      otaRejected_ = false;
      otaRejectReason_ = "";
      Serial.printf("OTA start: %s\n", upload.filename.c_str());
      if (!otaFilenameAllowed(upload.filename)) {
        otaRejected_ = true;
        otaRejectReason_ = "Wrong firmware project. Expected " +
                           expectedFirmwareFilename();
        Serial.println(otaRejectReason_);
        runtime_.showMessage("OTA reject");
        return;
      }
      runtime_.showMessage("OTA...");
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (otaRejected_) {
      return;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("OTA success: %u bytes\n", upload.totalSize);
        runtime_.showMessage("Rebooting");
      } else {
        Update.printError(Serial);
        runtime_.showMessage("OTA fail");
      }
    }
  }

  void handleUpdateFinish() {
    if (otaRejected_) {
      server_.send(400, "text/plain", otaRejectReason_);
      return;
    }
    if (Update.hasError()) {
      server_.send(500, "text/plain", Update.errorString());
    } else {
      server_.send(200, "text/plain", "OK");
      delay(1000);
      ESP.restart();
    }
  }

  void handleScan() {
    int count = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < count; i++) {
      if (i > 0) {
        json += ",";
      }
      json += "{\"ssid\":\"" + WiFi.SSID(i) +
              "\","
              "\"rssi\":" +
              String(WiFi.RSSI(i)) +
              ","
              "\"secure\":" +
              (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") +
              "}";
    }
    json += "]";
    WiFi.scanDelete();
    server_.send(200, "application/json", json);
  }

  void registerRoutes() {
    server_.on("/", [this]() { handleRoot(); });
    server_.on("/scan", HTTP_GET, [this]() { handleScan(); });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    server_.on("/update", HTTP_POST, [this]() { handleUpdateFinish(); },
               [this]() { handleUpdate(); });
    server_.onNotFound([this]() {
      server_.sendHeader("Location", "/");
      server_.send(302, "text/plain", "");
    });
  }

#ifdef WOKWI_SIM
  bool startWokwiStation() {
    esp_phy_erase_cal_data_in_nvs();

    runtime_.showMessage(" Wokwi...");
    Serial.println("Connecting to Wokwi-GUEST...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(wokwiGuestSsid());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < wokwiSetupTimeoutMs()) {
      delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wokwi-GUEST not found.");
      return false;
    }

    configMode_ = false;
    registerRoutes();
    server_.begin();
    project_.startPrograms();

    Serial.println("Wokwi ready.");
    Serial.println("Open http://localhost:8180 in your browser.");
    Serial.print("ESP IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
#endif

  void startConfigPortal() {
    configMode_ = true;

    WiFi.mode(WIFI_AP);
    String apSsid = setupApSsid();
    WiFi.softAP(apSsid.c_str());
    delay(500);

    dnsServer_.start(dnsPort(), "*", WiFi.softAPIP());
    registerRoutes();
    server_.begin();

    Serial.println("Config portal started.");
    Serial.print("AP SSID: ");
    Serial.println(apSsid);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    runtime_.beginSetup(apSsid.c_str());
  }

  bool connectToSavedWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(savedSsid_);

    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid_.c_str(), savedPass_.c_str());
    project_.startPrograms();

    unsigned long start = millis();
    unsigned long lastDotMs = start;
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < wifiTimeoutMs()) {
      project_.tickPrograms();
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
    runtime_.showBootIp(WiFi.localIP());
    project_.startPrograms();
    return true;
  }

  DisplayAdapter display_;
  RuntimeContext runtime_;
  Project project_;
  WebServer server_;
  DNSServer dnsServer_;
  String savedSsid_;
  String savedPass_;
  bool configMode_ = false;
  bool settingsWritable_ = true;
  bool otaRejected_ = false;
  String otaRejectReason_;
};

} // namespace tinker
