# Stage 1 migration baseline

Captured on 2026-09-12 before the firmware monorepo split.

## Repository and toolchain

- Git revision: `b4bdc6a7c58ff52132df51e3c3e9f72023612b8c`
- Working tree before baseline build: clean
- Project version: `2.0.4`
- Python: `3.9.6`
- PlatformIO Core: `6.1.19`
- Platform: `espressif32@7.0.1`
- Arduino framework: `3.20017.241212+sha.dcc1105b`
- Xtensa toolchain: `8.4.0+2021r2-patch5`
- MD_MAX72XX: `3.5.1`
- MD_Parola: `3.7.5`
- Wokwi extension: `3.7.0`

## Baseline build status

Command:

```bash
./scripts/build-firmware.sh
```

PlatformIO attempted both configured environments. Both compiled but failed while
linking:

- `esp32dev`: `.dram0.bss` overflowed by 72 bytes
- `wokwi`: `.dram0.bss` overflowed by 64 bytes

The largest application BSS symbol is the Maze Hero `Game` object at 65,444
bytes. This pre-existing failure is an accepted Stage 1 exception because
splitting unrelated programs and their static state is a primary goal of the
migration. No source-level memory workaround was applied to the monolith.

There is no newly linked `firmware.bin` whose size can be recorded. The latest
published `2.0.4` app binary is recorded below instead.

## Published 2.0.4 installer artifacts

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `docs/bootloader.bin` | 17,536 | `3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8` |
| `docs/partitions.bin` | 3,072 | `148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1` |
| `docs/firmware_2.0.4.bin` | 1,197,472 | `273b0fe9000df778c94ece74b6cdbda1001e9be1ea41c6f19cf3199addc6418e` |

The current `docs/` tree is approximately 27 MB. Its installer manifest uses:

- Chip family: `ESP32`
- Bootloader offset: `4096`
- Partition table offset: `32768`
- App offset: `65536`

## Pixel-art integrity baseline

- Generated files: 444
- Total bytes: 421,188
- Deterministic tree SHA-256:
  `8f1a03bcf9545e712dda827a2d740ac28fa9d9f23b0f4594639c93dae7f0d554`

The tree hash is calculated over files sorted by relative POSIX path. For each
file, hash the relative path, a null byte, then the exact file contents.

The source `pixel-art/` directory is gitignored and absent in a clean checkout.
The build importer preserves the checked-in generated catalog when no source
images are available.

## Hardware and Wokwi baseline

- Board: ESP32 DevKit C v4 / PlatformIO `esp32dev`
- Display: four chained red MAX7219 8x8 modules
- Layout: `fc16`
- DIN: GPIO 23
- CLK: GPIO 18
- CS: GPIO 5
- Power: ESP32 5V and GND
- Serial monitor: 9600 baud
- Wokwi port forwarding: `http://localhost:8180` to device port 80
- Wokwi firmware environment: `wokwi` with `WOKWI_SIM=1`

## Program baseline

Registration and rotation order:

1. `scroller`
2. `fireworks`
3. `maze_hero`
4. `pixel_art`
5. `weather_watch`
6. `real_weather`
7. `moon_phase`
8. `flight_watch`

Defaults:

- Selected programs: `fireworks`, `maze_hero`
- Program duration: 5 minutes
- Scroll message: `ESP32 Tinker` when the saved value is empty
- Scroll speed: 75 ms
- Fireworks launch delay: 60–2,000 ms
- Fireworks animation speed: 60 ms
- Display brightness: 0
- Fireworks maximum brightness: 15
- Maze width: 8–40
- Maze height: 4–24
- Maze Hero speed: 120–200 ms
- Weather postal code: empty
- Flight location: `40.6413`, `-73.7781`
- Flight radius: 25 miles
- Flight speed unit: knots

## Persistent settings baseline

NVS namespace: `esp32tinker`

| Key | Type | Purpose |
| --- | --- | --- |
| `ssid` | String | Wi-Fi SSID |
| `pass` | String | Wi-Fi password |
| `program` | String | Legacy/primary program |
| `programs` | uint8 | Selected-program bitmask |
| `progDurMin` | float | Program duration in minutes |
| `scrollMsg` | String | Scroller message |
| `scrollSpeed` | uint32 | Scroll speed |
| `fwMinDelayMs` | uint32 | Fireworks minimum launch delay |
| `fwMaxDelayMs` | uint32 | Fireworks maximum launch delay |
| `fwAnimMs` | uint32 | Fireworks animation speed |
| `mzMinW` | uint32 | Minimum maze width |
| `mzMaxW` | uint32 | Maximum maze width |
| `mzMinH` | uint32 | Minimum maze height |
| `mzMaxH` | uint32 | Maximum maze height |
| `mzHeroMinMs` | uint32 | Minimum Maze Hero delay |
| `mzHeroMaxMs` | uint32 | Maximum Maze Hero delay |
| `brightness` | uint8 | Display brightness |
| `fwMaxBright` | uint8 | Fireworks maximum brightness |
| `wxZip` | String | Weather postal code |
| `fltLat` | float | Flight latitude |
| `fltLon` | float | Flight longitude |
| `fltRad` | float | Flight radius |
| `fltRadUnit` | uint8 | Flight radius unit |
| `fltSpdUnit` | uint8 | Flight speed unit |

## Network and portal baseline

- With no saved credentials, hardware starts the captive portal.
- Setup AP name: `ESP32-Tinker-Setup-XXXX`, where `XXXX` is derived from MAC.
- Captive portal address: `http://192.168.4.1`
- Wokwi erases stale PHY calibration data and attempts `Wokwi-GUEST`.
- Successful Wokwi setup serves the portal at `http://localhost:8180`.
- Routes:
  - `GET /`: setup/config page
  - `GET /scan`: Wi-Fi scan JSON
  - `POST /save`: persist settings and reboot
  - `POST /update`: OTA upload and reboot
  - Other paths redirect to `/`
- Browser form persistence key: `esp32TinkerSetup`
- OTA accepts an arbitrary uploaded app binary and does not currently validate a
  project identity.

## Manual baseline limitation

A Wokwi visual baseline cannot be launched from this revision because neither
environment links. Visual acceptance is deferred until the split produces a
linkable `justin` Wokwi build.
