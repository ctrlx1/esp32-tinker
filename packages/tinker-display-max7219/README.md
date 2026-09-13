# tinker-display-max7219

Shared MAX7219 display adapter for ESP32 Tinker firmware projects.

Each project supplies a module type, chip-select pin, and module count. The
adapter owns one `MD_Parola` instance and exposes text animation, brightness,
boot status, and framebuffer drawing through `tinker::RuntimeContext`.

The current MAX7219 projects use FC16 modules, GPIO 5 chip select, and four
chained 8×8 modules. Hardware SPI continues to use the ESP32 board defaults
(MOSI GPIO 23 and clock GPIO 18).

Consumers must register both local packages in `platformio.ini`:

```ini
lib_deps =
  symlink://../../packages/tinker-core
  symlink://../../packages/tinker-display-max7219
```

The adapter supports Generic, FC16, Parola, and ICStation module layouts over
hardware SPI. Brightness values use the MAX7219 range of 0–15. Its runtime
context is borrowed and must not outlive the adapter.
