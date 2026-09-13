#include "../hardware/max7219_profile.h"
#include "moon_phase_project.h"

#include <tinker/max7219_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Max7219Display, MoonPhaseProject> app(
    moon_phase_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
