#pragma once

#include <stdint.h>
#include <tinker/runtime_context.h>

namespace aircraft3d {

constexpr uint8_t kAircraftCount = 4;

void reset(uint8_t &aircraft, float &yaw, float &rotor);
void advance(uint8_t &aircraft, float &yaw, float &rotor, unsigned long dtMs);
void draw(tinker::RuntimeContext &runtime, uint8_t aircraft, float yaw,
          float rotor);

} // namespace aircraft3d
