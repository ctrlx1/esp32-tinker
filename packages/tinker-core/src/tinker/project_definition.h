#pragma once

#include "settings_schema.h"

namespace tinker {

struct ProjectDefinition {
  const char *id;
  const char *name;
  const char *version;
  const char *setupApSsidPrefix;
  SettingsSchema settings;
};

} // namespace tinker
