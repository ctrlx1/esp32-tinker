#pragma once

#include "flight_info_settings.h"

#include <stddef.h>
#include <stdint.h>

namespace adsb {

constexpr uint8_t kMaxAircraft = 5;

enum class Status : uint8_t {
  Loading,
  Success,
  WifiUnavailable,
  HttpFailed,
  ParseFailed,
};

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

void start(const FlightInfoSettings &settings);
void tick(const FlightInfoSettings &settings);

const Aircraft *items();
uint8_t count();
Status status();

bool airlineNameFromCallsign(const char *callsign, char *out, size_t outSize);

} // namespace adsb
