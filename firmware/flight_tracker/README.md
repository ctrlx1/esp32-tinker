# Flight Tracker

ESP32 Tinker firmware for a P4-256x128-2121-A5 HUB75 panel (64×32 RGB,
1/16 scan). The live program is a north-up radar: you are the center
dot, nearby aircraft from [adsb.lol](https://api.adsb.lol) are drawn as
small heading-oriented planes, and the circle is the configured search
radius mapped to the long axis of the panel (`max(width, height) / 2`
pixels). Planes clip per-pixel at the circle and at the panel edges.

The earlier rotating 3D aircraft cycle is parked in `parked/` and is not
compiled. The portal matches Flight Watch search settings (lat/lon,
radius, units, brightness), plus an Open in Google Maps link and a
Paste GPS button for comma-separated coordinates.

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
after changing `diagram.json` or the chip binary. Wokwi connects to
`Wokwi-GUEST` and fetches the same adsb.lol feed as production. If that
request fails, it falls back to a few location-seeded demo tracks.

The Wokwi build fills the panel red as soon as the display adapter starts,
then continues into the normal boot/version/radar path. Serial reports
`HUB75 Wokwi: GPIO bit-bang`.
