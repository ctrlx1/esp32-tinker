#include "../hardware/hub75_profile.h"
#include "starter_project.h"

#include <tinker/hub75_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Hub75Display, StarterProject> app(
    starter_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
