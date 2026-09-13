#pragma once

#include "../flight_watch_settings.h"

#include <tinker/runtime_context.h>

using ProgramConfig = FlightWatchSettings;

void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();
