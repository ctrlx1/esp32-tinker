#pragma once

#include <stdint.h>

namespace tinker {

struct SettingsSchema {
  const char *nvsNamespace;
  const char *versionKey;
  uint16_t currentVersion;

  bool isVersioned() const {
    return versionKey != nullptr && versionKey[0] != '\0';
  }
};

} // namespace tinker
