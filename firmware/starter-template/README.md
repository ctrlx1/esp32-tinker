# Firmware Starter Template

This is a hardware-neutral scaffold, not a buildable firmware project. It
documents the boundary a new ESP32 Tinker project must implement before setting
`buildable` to `true`.

## Create a project

1. Copy this directory to `firmware/<project-id>`.
2. Set `VERSION` to the new project's initial semantic version.
3. Rename the `.example` source and portal files and replace every `TODO`.
4. Add `platformio.ini` with the board, framework, pinned dependencies,
   production environment, and a Wokwi environment (see [Wokwi](#wokwi)).
5. Add `hardware/<profile>.h` containing the board, pins, geometry, power notes,
   and concrete display adapter configuration.
6. Add Wokwi files in the same change as the hardware profile. Do not leave a
   new buildable project without a simulator target.
7. Choose a unique NVS namespace no longer than 15 characters and define a
   settings version/migration.
8. Add a unique entry to `projects.json`, including:
   - `id`, `name`, `description`, `path`, and `versionFile`;
   - `buildable: true`;
   - production **and** Wokwi environments (`"wokwi": "wokwi"`);
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
./scripts/build.sh <project-id> --env wokwi
```

Use `firmware/starter-max7219` as the complete runnable MAX7219 reference, or
`firmware/flight_tracker` for HUB75.

## Wokwi

Every project copied from this template must ship a simulator target. Wokwi is
how the repo develops without hardware; `./scripts/build.sh <project> --env wokwi`
is the development-mode command.

Required files (rename the `.example` copies and replace every `TODO`):

- `platformio.ini` `[env:wokwi]` with `build_flags = -D WOKWI_SIM=1`
- `wokwi.toml` pointing at `.pio/build/wokwi/firmware.bin` and forwarding
  `localhost:8180` to the device HTTP port
- `diagram.json` with `board-esp32-devkit-c-v4`, the display part, and wires
  that match `hardware/<profile>.h` (do not copy another project's pin map)

Register `"wokwi": "wokwi"` under `environments` in `projects.json`. CI builds
every registered environment, so a missing Wokwi env is an incomplete project.

If Wokwi has no official part for the panel, still add the diagram using the
closest unofficial or custom-chip part and note that in the project README.
`starter-max7219` uses `wokwi-max7219-matrix`; `flight_tracker` uses a custom
`chip-hub75-matrix` because Wokwi does not emulate HUB75 I2S DMA.

After building the Wokwi firmware, select `firmware/<project-id>/wokwi.toml`,
start the simulator, and open `http://localhost:8180`. The firmware joins
`Wokwi-GUEST` when `WOKWI_SIM=1` is set.

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

`starter-template` has no `platformio.ini`, concrete adapter, or publishable
artifacts. Wokwi files are `.example` scaffolds only. Dependency checks report
it as an intentionally skipped hardware-unassigned template, and direct
build/publish commands fail clearly.
