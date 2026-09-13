# Weather Watch

Standalone ESP32 Tinker firmware that cycles through animated sunny, cloudy,
rainy, stormy, and snowy scenes on the four-module FC16 MAX7219 display.

Hardware:

- ESP32 DevKit C / PlatformIO `esp32dev`
- four chained FC16 MAX7219 8x8 modules
- DIN GPIO 23, CLK GPIO 18, CS GPIO 5
- default ESP32 partition table

The captive portal stores only display brightness (0–15), in addition to the
shared Wi-Fi credentials. Settings use the versioned `weather_watch` NVS
namespace with schema key `cfgVer`.

From the repository root:

```bash
./scripts/check-deps.sh weather_watch
./scripts/build.sh weather_watch
./scripts/build.sh weather_watch --env wokwi
```

The local `build.sh` resolves the repository root and delegates to
`./scripts/build.sh weather_watch`, forwarding any additional arguments. For
Wokwi, select `firmware/weather_watch/wokwi.toml`, start the simulator, and
open `http://localhost:8180`.
