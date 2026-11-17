# Moon HUB75

Lunar phase display for a 64×32 HUB75 RGB panel (P4-256x128-2121-A5,
FM6124HJ / FM6124HU, 1/16 scan).

Startup shows the wolf-and-moon picture, then the title “Phases / Of The /
Moon,” and clears that title with one of five random animations. After that,
a 32×32 moon stays on the right and is shaded to the current phase. With a
saved place, the bright limb points toward the Sun in that sky, so the top
of the disk is up from the horizon. Without a place, waxing stays bright on
the right and waning on the left. The left column
scrolls the current phase, illumination, and the time until the next new
moon, first quarter, full moon, and last quarter. Each phrase starts at the
left of its own line. After that it shows today's moonrise and moonset, a
Distance label in miles, kilometers, or both, and the current azimuth and altitude. Those lines appear once the setup page has a
latitude, longitude, and timezone offset. The clock comes from NTP. Until the time is
valid, the phase cycles once per minute so the panel still moves.

The setup page stores display brightness (default `4`), scroll speed
(default `75 ms`), the scroll-text color (default Ice), text brightness,
and the moon distance unit (default Both) in the `moon_hub75` NVS namespace.
Text brightness starts at the panel brightness and can be lowered, down to
0, without dimming the moon. The color
list is Ice, Silver, White, Gold, Amber, Rose, Mint, Violet, Light Blue,
Blue, and Dark Blue. Distance can be Both, Miles, or Kilometers. Latitude,
longitude, and the timezone offset are saved there too. Leave them blank to
keep the place already stored.

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
