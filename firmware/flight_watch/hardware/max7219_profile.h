#pragma once

#include <tinker/max7219_display.h>

namespace flight_watch_hardware {

constexpr uint8_t kHardwareSpiMosiPin = 23;
constexpr uint8_t kHardwareSpiClockPin = 18;
constexpr uint8_t kChipSelectPin = 5;
constexpr uint8_t kModuleCount = 4;
constexpr uint16_t kDisplayWidth = kModuleCount * 8;
constexpr uint8_t kDisplayHeight = 8;

constexpr tinker::Max7219DisplayConfig kDisplayConfig = {
    tinker::Max7219ModuleType::Fc16,
    kChipSelectPin,
    kModuleCount,
};

} // namespace flight_watch_hardware
