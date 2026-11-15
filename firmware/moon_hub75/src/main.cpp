#include "../hardware/hub75_profile.h"
#include "moon_hub75_project.h"

#include <tinker/hub75_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Hub75Display, MoonHub75Project> app(
    moon_hub75_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
