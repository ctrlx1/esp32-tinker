# tinker-display-hub75

Shared HUB75 RGB matrix adapter for ESP32 Tinker firmware projects.

Each project supplies panel geometry, the HUB75 GPIO map, and the shift
driver (`ShiftRegister` or `Fm6124`). The adapter owns
one `MatrixPanel_I2S_DMA` instance and exposes text, brightness, boot status,
and RGB drawing through `tinker::RuntimeContext`.

`fillColor` and `setPixelColor` present immediately unless the caller wraps them
in `beginColorFrame` / `endColorFrame`, which batch draws and flush once.
`blitRgb565` copies a full RGB565 bitmap in one call; use it for offscreen
compose. Production firmware enables DMA double-buffering and flips after each
present so the scanning panel does not show mid-frame writes.

The Wokwi backend is a GPIO stand-in, not the hardware I2S DMA driver. It sends
4 color bits per channel, skips unchanged scan rows, and does not flush again
on brightness-only updates. Rebuild `wokwi/hub75-matrix.chip.wasm` after
changing `HUB75_WOKWI_COLOR_BITS`. Production firmware still draws full RGB565
through `MatrixPanel_I2S_DMA`.

Brightness values use the shared portal range of 0–15 and are mapped onto the
DMA driver’s 0–255 scale. The 1-bit framebuffer group is intentionally unset.

Consumers must register both local packages in `platformio.ini`:

```ini
lib_deps =
  symlink://../../packages/tinker-core
  symlink://../../packages/tinker-display-hub75
```

Its runtime context is borrowed and must not outlive the adapter.

Wokwi cannot emulate the I2S LCD/DMA path this adapter uses on hardware. The
`wokwi/` custom chip plus the `WOKWI_SIM` GPIO bit-bang backend are the
supported simulator stand-in. Rebuild `wokwi/hub75-matrix.chip.wasm` with
`wokwi/build-chip.sh` after changing the chip source.
