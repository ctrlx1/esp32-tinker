# Flight Watch

Standalone ESP32 Tinker firmware that displays nearby aircraft on a four-module
FC16 MAX7219 matrix.

## Hardware

- ESP32 DevKit C / PlatformIO `esp32dev`
- Four chained FC16 MAX7219 modules
- DIN GPIO 23, CLK GPIO 18, CS GPIO 5
- Default ESP32 partition table

## Settings

The captive portal configures latitude, longitude, search radius and radius
unit, displayed speed unit, brightness, and scroll speed. The search radius is
strictly limited to 250 nautical miles after converting miles or kilometres.
Settings use the `flight_watch` NVS namespace with schema version 1.

The aircraft request runs in one controlled FreeRTOS worker so HTTP waits never
block the Arduino loop. Up to three aircraft are handed back to the main task;
only the main task updates the display.

Build it from the repository root:

```bash
./scripts/build.sh flight_watch
./scripts/build.sh flight_watch --env wokwi
```

The local `build.sh` delegates to the same registered build command.

For Wokwi, select `firmware/flight_watch/wokwi.toml`, start the simulator, and
open `http://localhost:8180`.
