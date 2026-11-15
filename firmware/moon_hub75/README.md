# Moon HUB75

Lunar phase display for a 64×32 HUB75 RGB panel (P4-256x128-2121-A5,
FM6124HJ / FM6124HU, 1/16 scan).

A 32×32 moon stays on the right and is shaded to the current phase, bright
on the right while waxing and on the left while waning. The left column
scrolls the current phase, illumination, and the time until the next new
moon, first quarter, full moon, and last quarter. The clock comes from NTP. Until the time is
valid, the phase cycles once per minute so the panel still moves.

The setup page stores display brightness (default `4`) and scroll speed
(default `75 ms`) in the `moon_hub75` NVS namespace.

From the repository root:

```bash
./scripts/check-deps.sh moon_hub75
./scripts/build.sh moon_hub75
./scripts/build.sh moon_hub75 --env wokwi
```

For Wokwi, select `firmware/moon_hub75/wokwi.toml`, start the simulator,
and open `http://localhost:8180`. The diagram uses a custom `chip-hub75-matrix`
because Wokwi does not emulate HUB75 I2S DMA. FM6124 timing applies on the
real panel only.
