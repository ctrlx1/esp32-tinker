#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class JustinDisplay {
public:
  void begin();
  void setBrightness(uint8_t brightness);
  void showVersion(const char *version);
  void showIp(const IPAddress &address);
  void showMessage(const char *message);
  void beginSetup(const char *apSsid);
  void tickSetup();

private:
  char setupScrollBuffer_[48] = "";
};
