# Starter HUB75

Minimal runnable ESP32 Tinker firmware for a 64×32 HUB75 RGB panel
(P4-256x128-2121-A5, 1/16 scan).

It demonstrates:

- `tinker-core` Wi-Fi, captive portal, settings, and OTA lifecycle;
- the shared `tinker-display-hub75` adapter;
- a project-owned hardware profile and version;
- a looping Hello World whose color cycles through the hue wheel.

From the repository root:

```bash
./scripts/check-deps.sh starter-hub75
./scripts/build.sh starter-hub75
./scripts/build.sh starter-hub75 --env wokwi
```

For Wokwi, select `firmware/starter-hub75/wokwi.toml`, start the simulator,
and open `http://localhost:8180`. The diagram uses a custom `chip-hub75-matrix`
because Wokwi does not emulate HUB75 I2S DMA.
