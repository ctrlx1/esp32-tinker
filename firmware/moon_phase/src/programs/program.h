#pragma once

#include "../moon_phase_settings.h"

#include <tinker/runtime_context.h>

using ProgramConfig = MoonPhaseSettings;

void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();
