# moon_phase

Standalone ESP32 firmware that displays the current lunar phase on a
four-module FC16 MAX7219 matrix. It vertically pans a full 32-pixel moon, then
scrolls the phase name, illumination, and time to the next major phases.

## Hardware

- Board: PlatformIO `esp32dev`
- Display: FC16 MAX7219, four chained 8x8 modules
- DIN: GPIO 23
- CLK: GPIO 18
- CS: GPIO 5

## Settings

The captive portal stores only display brightness (default `0`) and phase-name
scroll speed (default `75 ms`) in the versioned `moon_phase` NVS namespace.
Wi-Fi credentials are managed by the shared runtime in the same namespace.

Time synchronization is asynchronous. The firmware continues its demo lunar
cycle while NTP is unavailable and switches to the UTC-derived phase once the
ESP32 clock becomes valid.

## Build and simulate

From this directory:

```bash
./build.sh
./build.sh --env wokwi
```

For Wokwi, select `wokwi.toml`, start the simulator, and open
`http://localhost:8180`.
