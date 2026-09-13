# Stage 8 hardware inventory

The hardware profile for all four standalone extractions was confirmed before
implementation. `moon_phase`, `weather_watch`, `real_weather`, and
`flight_watch` initially use the same reference setup as `justin`.

## Confirmed reference profile

- PlatformIO board: `esp32dev` (ESP32 Dev Module)
- Arduino framework and platform: `espressif32@7.0.1`
- Display: four chained FC16 MAX7219 8×8 modules (32×8 total)
- MAX7219 DIN: GPIO 23 (ESP32 hardware-SPI MOSI)
- MAX7219 CLK: GPIO 18 (ESP32 hardware-SPI clock)
- MAX7219 CS: GPIO 5
- Logic/display supply: 5 V with a common ESP32/display ground
- Partition layout: the existing ESP32 default layout
  - bootloader offset: 4096
  - partition table offset: 32768
  - application offset: 65536
- Simulator: supported using each project's own Wokwi diagram/config

The Wokwi diagrams model the display from the ESP32 5 V pin. A physical
installation must use a 5 V supply with enough current for four matrices; the
repository does not encode or validate supply capacity.

No extra peripherals were identified for these four projects. If a physical
build diverges later, it receives a new project-owned hardware profile and
matching Wokwi configuration rather than silently changing this reference
profile.
