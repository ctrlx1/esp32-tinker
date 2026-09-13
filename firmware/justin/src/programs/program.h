#pragma once

#include "../justin_settings.h"

#include <tinker/runtime_context.h>

using ProgramConfig = JustinSettings;

ProgramId parseProgramId(const String &value);
const char *programIdToString(ProgramId id);
uint8_t programIdToFlag(ProgramId id);
void setProgramRuntimeContext(tinker::RuntimeContext &runtime);
tinker::RuntimeContext &programRuntimeContext();

