#pragma once

#include "../real_weather_settings.h"

#include <tinker/runtime_context.h>

using ProgramConfig = RealWeatherSettings;

void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();
