#include <tinker/max7219_display.h>
#include <tinker/tinker_app.h>

#include "justin_project.h"

namespace {

constexpr tinker::Max7219DisplayConfig kDisplayConfig = {
    tinker::Max7219ModuleType::Fc16,
    5,
    4,
};

tinker::TinkerApp<tinker::Max7219Display, JustinProject> app(kDisplayConfig);

} // namespace

void setup() { app.setup(); }
void loop() { app.loop(); }
