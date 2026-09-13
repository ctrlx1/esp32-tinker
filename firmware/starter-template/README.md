# Firmware Starter Template

This is a hardware-neutral scaffold, not a buildable firmware project. It
documents the boundary a new ESP32 Tinker project must implement before setting
`buildable` to `true`.

## Create a project

1. Copy this directory to `firmware/<project-id>`.
2. Set `VERSION` to the new project's initial semantic version.
3. Rename the `.example` source and portal files and replace every `TODO`.
4. Add `platformio.ini` with the board, framework, pinned dependencies, and
   production environment.
5. Add `hardware/<profile>.h` containing the board, pins, geometry, power notes,
   and concrete display adapter configuration.
6. Add `wokwi.toml` and `diagram.json` only when the hardware is supported by
   Wokwi.
7. Choose a unique NVS namespace no longer than 15 characters and define a
   settings version/migration.
8. Add a unique entry to `projects.json`, including:
   - `id`, `name`, `description`, `path`, and `versionFile`;
   - `buildable: true`;
   - production and optional Wokwi environments;
   - configured hardware profile and chip family;
   - dependency and pre-build metadata;
   - unique docs route/path;
   - bootloader, partition, and app artifact metadata.
9. Add a thin `build.sh` that delegates to
   `../../scripts/build.sh <project-id>`.
10. Run from the repository root:

```bash
./scripts/check-deps.sh <project-id>
./scripts/build.sh <project-id>
```

Use `firmware/starter-max7219` as the complete runnable reference.

## Runtime contract

A `TinkerApp<DisplayAdapter, Project>` display adapter supplies:

- `using Config = ...`;
- a constructor accepting that configuration;
- `begin()`;
- `runtimeContext()`.

The returned `tinker::RuntimeContext` may provide text, brightness, boot-status,
and framebuffer capability groups. Missing groups are safe, but a program must
check that the capability it needs is present.

The project class receives the runtime context and implements:

- `definition()` and an NVS settings schema;
- `migrateSettings()`, `loadSettings()`, and `saveSettings()`;
- `displayBrightness()`;
- `buildPortalPage()` and `applyPortalRequest()`;
- `startPrograms()` and `tickPrograms()`.

The project and adapter must outlive their borrowed runtime context. Keep
project programs/settings and hardware-specific code inside the new firmware
directory.

## Intentional status

`starter-template` has no `platformio.ini`, concrete adapter, simulator, or
publishable artifacts. Dependency checks report it as an intentionally skipped
hardware-unassigned template, and direct build/publish commands fail clearly.
