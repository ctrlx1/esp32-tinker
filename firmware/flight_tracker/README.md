# Flight Tracker

Minimal ESP32 Tinker firmware for a P4-256x128-2121-A5 HUB75 panel
(64×32 RGB, 1/16 scan). The first program cycles four low-poly aircraft
(airliner, fighter, high-wing prop, helicopter) that rotate in 3D. The
helicopter rotor and prop spinner keep turning while each model completes
one full turntable revolution before the next aircraft appears.

Power the panel from a dedicated 5 V / ≥4 A supply on the VH4 header and
share ground with the ESP32. Do not feed LED power from the DevKit.
GPIO 12 is a strapping pin used as G2 in the default map.

From the repository root:

```bash
./scripts/check-deps.sh flight_tracker
./scripts/build.sh flight_tracker
./scripts/build.sh flight_tracker --env wokwi
```

For Wokwi, select `firmware/flight_tracker/wokwi.toml`, start the simulator,
and open `http://localhost:8180`. Wokwi does not emulate ESP32 I2S LCD/DMA and
does not officially support HUB75, so the diagram uses a custom
`chip-hub75-matrix` framebuffer wired to the same GPIOs as the hardware
profile. The Wokwi firmware bit-bangs those pins instead of starting the DMA
driver. Unused address `E` is tied to GND. Stop and restart the simulator
after changing `diagram.json` or the chip binary.

The Wokwi build fills the panel red as soon as the display adapter starts,
then continues into the normal boot/version/aircraft path. Serial reports
`HUB75 Wokwi: GPIO bit-bang`.
