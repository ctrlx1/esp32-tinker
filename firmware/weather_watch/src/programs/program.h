#pragma once

#include "../weather_watch_settings.h"

#include <tinker/runtime_context.h>

using ProgramConfig = WeatherWatchSettings;

void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();
