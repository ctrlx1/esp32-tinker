#pragma once

#include <tinker/hub75_display.h>

namespace starter_hardware {

// P4-256x128-2121-A5 is a 64×32 HUB75 indoor module (256mm × 128mm, 1/16 scan).
// GPIO 12 (G2) is an ESP32 strapping pin; keep it stable during boot.
constexpr uint16_t kDisplayWidth = 64;
constexpr uint8_t kDisplayHeight = 32;

constexpr int8_t kR1Pin = 25;
constexpr int8_t kG1Pin = 26;
constexpr int8_t kB1Pin = 27;
constexpr int8_t kR2Pin = 14;
constexpr int8_t kG2Pin = 12;
constexpr int8_t kB2Pin = 13;
constexpr int8_t kAPin = 23;
constexpr int8_t kBPin = 19;
constexpr int8_t kCPin = 5;
constexpr int8_t kDPin = 17;
constexpr int8_t kEPin = -1;
constexpr int8_t kLatPin = 4;
constexpr int8_t kOePin = 15;
constexpr int8_t kClkPin = 16;

constexpr tinker::Hub75DisplayConfig kDisplayConfig = {
    kDisplayWidth, kDisplayHeight, kR1Pin,   kG1Pin, kB1Pin, kR2Pin,
    kG2Pin,        kB2Pin,         kAPin,    kBPin,  kCPin,  kDPin,
    kEPin,         kLatPin,        kOePin,   kClkPin,
    tinker::Hub75Driver::ShiftRegister,
};

} // namespace starter_hardware
