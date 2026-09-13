# Real Weather

Standalone ESP32 Tinker firmware that scrolls current conditions and a
seven-day Open-Meteo forecast for a configured US ZIP code.

Hardware:

- ESP32 `esp32dev`
- four chained FC16 MAX7219 modules
- DIN GPIO 23, CLK GPIO 18, CS GPIO 5
- default ESP32 partition layout

Settings are stored in the `real_weather` NVS namespace with schema key
`cfgVer` at version 1. Defaults match the original program: an empty ZIP code,
brightness 0, and a 75 ms scroll speed.

The setup portal accepts an empty ZIP, a five-digit US ZIP, or ZIP+4. It also
strictly validates brightness from 0 through 15 and integer scroll speed from
1 through 10000 ms.

Build from this directory:

```bash
./build.sh
./build.sh --env wokwi
```

For Wokwi, open `wokwi.toml`, start the simulation, and visit
`http://localhost:8180`.

Weather HTTP requests and JSON parsing run on one fixed FreeRTOS worker. The
Arduino loop remains responsible for all display operations. Response bodies,
requests, and published display messages are bounded.
