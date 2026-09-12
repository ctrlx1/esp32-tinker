# tinker-core

Shared ESP32 Arduino runtime used by the firmware projects in this repository.
PlatformIO consumes it as a local `symlink://` dependency, so edits are compiled
directly from `packages/tinker-core`.

The core owns Wi-Fi station/AP setup, captive-portal and OTA routes, NVS
sessions, versioned settings hooks, descriptor-based program scheduling, and
capability-based display transitions. A firmware project provides:

- a `ProjectDefinition` and settings load/save/migration hooks;
- portal rendering and validation;
- a stable `ProgramDescriptor` table and program callbacks;
- a hardware adapter implementing the display operations used by `TinkerApp`.

Descriptor tables, callback contexts, clock/transition callbacks, and program
ordering must remain stable for a scheduler run. Descriptor indices are
persisted as selection-mask bits, so reordering descriptors requires a project
settings migration.

Versioned settings migrations run with writable `Preferences` and should be
idempotent. The core does not advance the version marker when migration fails,
and it disables settings writes when migration fails or stored settings come
from a newer firmware. The existing `justin` project intentionally uses the
legacy unversioned schema to preserve its `esp32tinker` NVS layout byte-for-byte.

The interfaces use plain structs, templates, and function pointers rather than
virtual classes. This keeps allocation and static RAM costs explicit on ESP32.
