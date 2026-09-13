#include "../hardware/hub75_profile.h"
#include "flight_info_project.h"

#include <tinker/hub75_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Hub75Display, FlightInfoProject> app(
    flight_info_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
