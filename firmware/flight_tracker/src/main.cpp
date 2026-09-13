#include "../hardware/hub75_profile.h"
#include "flight_tracker_project.h"

#include <tinker/hub75_display.h>
#include <tinker/tinker_app.h>

namespace {
tinker::TinkerApp<tinker::Hub75Display, FlightTrackerProject> app(
    flight_tracker_hardware::kDisplayConfig);
}

void setup() { app.setup(); }

void loop() { app.loop(); }
