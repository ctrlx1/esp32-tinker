# Flight Info

ESP32 Tinker firmware for a P4-256x128-2121-A5 HUB75 panel (64×32 RGB,
1/16 scan, FM6124HJ / FM6124HU drivers). The live program cycles a full-color info card for nearby
aircraft from [adsb.lol](https://api.adsb.lol): callsign, airline, type,
altitude, speed, track, climb/descend, and distance from the viewer.

It is the HUB75 counterpart to Flight Watch. Up to ten nearest aircraft
are queued and shown one at a time for a configurable dwell (default 6
seconds). The captive portal matches Flight Tracker search settings
(lat/lon, radius, units, brightness), plus card dwell, distance units, an Open in Google
Maps link, and a Paste GPS button.

Power the panel from a dedicated 5 V / ≥4 A supply on the VH4 header and
share ground with the ESP32. Do not feed LED power from the DevKit.
GPIO 12 is a strapping pin used as G2 in the default map.

From the repository root:

```bash
./scripts/check-deps.sh flight_info
./scripts/build.sh flight_info
./scripts/build.sh flight_info --env wokwi
```

For Wokwi, select `firmware/flight_info/wokwi.toml`, start the simulator,
and open `http://localhost:8180`. Wokwi does not emulate ESP32 I2S LCD/DMA
and does not officially support HUB75, so the diagram uses a custom
`chip-hub75-matrix` framebuffer wired to the same GPIOs as the hardware
profile. The Wokwi firmware bit-bangs those pins instead of starting the DMA
driver. Unused address `E` is tied to GND. Stop and restart the simulator
after changing `diagram.json` or the chip binary. Wokwi connects to
`Wokwi-GUEST` and fetches the same adsb.lol feed as production. If that
request fails, it falls back to a few demo aircraft cards.
