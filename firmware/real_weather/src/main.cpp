#include "../hardware/max7219_profile.h"
#include "real_weather_project.h"

#include <tinker/max7219_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Max7219Display, RealWeatherProject> app(
    real_weather_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
