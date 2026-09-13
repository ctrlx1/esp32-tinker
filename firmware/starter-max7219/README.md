# Starter MAX7219

Minimal runnable ESP32 Tinker firmware for the four-module FC16 MAX7219
reference hardware.

It demonstrates:

- `tinker-core` Wi-Fi, captive portal, settings, and OTA lifecycle;
- the shared `tinker-display-max7219` adapter;
- a project-owned hardware profile and version;
- a looping configurable “Hello World” text program.

From the repository root:

```bash
./scripts/check-deps.sh starter-max7219
./scripts/build.sh starter-max7219
./scripts/build.sh starter-max7219 --env wokwi
```

For Wokwi, select `firmware/starter-max7219/wokwi.toml`, start the simulator,
and open `http://localhost:8180`.
