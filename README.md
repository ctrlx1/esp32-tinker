# ESP32 Tinker

An ESP32 tinkering platform built around a 4-module MAX7219 LED matrix. Flash from the browser, configure over Wi-Fi, and iterate on firmware without an IDE after the first flash.

---

## ⚡ [Install via browser](https://justinmahar.github.io/esp32-tinker/) — no IDE needed

---

## Features

- **Captive portal setup** — on first boot the ESP32 broadcasts a hotspot; connect and configure from your phone or browser
- **Persistent config** — settings saved to NVS flash, survive reboots and firmware updates
- **Wi-Fi scanner** — scan nearby networks and tap to auto-fill the SSID field
- **IP display on boot** — scrolls your local IP across the matrix so you always know where to reach the config page
- **In-browser reconfigure** — visit the device IP any time to change Wi-Fi or device settings
- **OTA firmware updates** — upload a compiled `.bin` from the config page, no USB cable needed after first flash
- **Wokwi simulator** — develop and test without hardware (see [Development](#development))

---

## Hardware

### Electronics

| Component                      | Link                                                    |
| ------------------------------ | ------------------------------------------------------- |
| ESP32 Dev Board                | [AliExpress](https://s.click.aliexpress.com/e/_opLIWvk) |
| MAX7219 Dot Matrix (4 modules) | [AliExpress](https://s.click.aliexpress.com/e/_oo3TdS6) |

### Enclosure hardware

| Component         | Link                                                     |
| ----------------- | -------------------------------------------------------- |
| M3 Thread Inserts | [AliExpress](https://s.click.aliexpress.com/e/_c2Iun0o1) |
| M3x8 Screws       | [AliExpress](https://s.click.aliexpress.com/e/_oogbRPM)  |
| Acrylic Sheet     | [AliExpress](https://s.click.aliexpress.com/e/_oorPPai)  |
| 6x3mm Magnets     | [AliExpress](https://s.click.aliexpress.com/e/_c3qP6N2t) |

### Battery / wireless (optional)

| Component     | Link                                                     |
| ------------- | -------------------------------------------------------- |
| Battery       | [AliExpress](https://s.click.aliexpress.com/e/_opFpjhg)  |
| BMS           | [AliExpress](https://s.click.aliexpress.com/e/_c4sosls1) |
| On/Off Switch | [AliExpress](https://s.click.aliexpress.com/e/_oDqU0l8)  |

> Hardware affiliate links are from the original [YouTube Subscriber Counter](https://github.com/ThePrintingPilot/YouTube-Subscriber-Counter) project by [The Printing Pilot](https://github.com/ThePrintingPilot) — they help support that project at no extra cost to you.

3D enclosure files:

[![Printables](https://img.shields.io/badge/Printables-FA6831?style=for-the-badge&logoColor=white)](https://www.printables.com/model/1756251-youtube-subscriber-v20)
[![MakerWorld](https://img.shields.io/badge/MakerWorld-000000?style=for-the-badge&logoColor=white)](https://makerworld.com/en/models/2941691-youtube-subscriber-v2-0#profileId-3294669)

Circuit wiring is defined in `firmware/justin/diagram.json` (DIN→GPIO23, CLK→GPIO18, CS→GPIO5, power via `V+` / `GND.2`).

---

## First-time setup

### 1. Flash the firmware

**Option A — Browser installer (recommended)**

No IDE needed. Connect your ESP32 via USB and hit Install:

➡ **[Install via browser](https://justinmahar.github.io/esp32-tinker/)**

Requires Chrome or Edge on desktop.

**Option B — PlatformIO**

1. Open the `firmware/justin` folder in VS Code or Cursor (PlatformIO extension required)
2. Build and upload to your ESP32 board

### 2. Configure via the portal

1. Power on the device — the matrix shows `Setup`
2. Connect to the Wi-Fi network shown on the matrix — **`Tinker-Setup-XXXX`** (unique 4-character suffix per device)
3. A browser page opens automatically (or navigate to `192.168.4.1`)
4. Tap **Scan for networks**, pick your Wi-Fi, enter your password, and save

---

## After setup

On every boot the device connects to your saved Wi-Fi and scrolls the assigned IP address across the matrix. Open that IP in any browser on the same network to:

- Change Wi-Fi network or password
- Update device settings
- Upload new firmware (`.bin`) without a USB cable

Leave the Wi-Fi fields blank when saving and the device keeps the previously stored network values.

---

## OTA firmware updates

1. Build with PlatformIO — the OTA `.bin` is at `firmware/justin/.pio/build/esp32dev/firmware.bin`
2. Open the device IP in your browser
3. Scroll to **Firmware update**, pick the main firmware `.bin`, click **Upload firmware**
4. The matrix shows `OTA...` then `Rebooting` — done

---

## Development

Firmware projects are registered in `projects.json`. The current firmware source lives in `firmware/justin/`; open that folder as your Cursor/VS Code workspace when simulating.

### Shared firmware runtime

`packages/tinker-core` is a local PlatformIO library shared by firmware
projects. It owns the ESP32 Wi-Fi/AP lifecycle, captive-portal routes, NVS
sessions and schema hooks, OTA uploads, program descriptors/scheduling, and
display-transition algorithms.

Each project supplies its own project definition, settings validation, portal
fields, programs, and hardware adapter. The `justin` project still owns its
MAX7219/MD_Parola implementation; that driver will move to the dedicated
display package in the next migration stage.

### Check dependencies

Verify Python, PlatformIO, project-specific tools, and supported simulator environments:

```bash
./scripts/check-deps.sh                        # every project and environment
./scripts/check-deps.sh justin                 # one project
./scripts/check-deps.sh justin --env production
./scripts/check-deps.sh all --env wokwi
```

The checker reads `projects.json`, exits non-zero if the requested scope is not ready, and prints install hints.

The project version lives in `firmware/justin/VERSION`. Increment it with:

```bash
./scripts/bump-version.sh justin           # patch bump
./scripts/bump-version.sh justin --minor
./scripts/bump-version.sh justin --major
```

### Build and flash (PlatformIO)

```bash
./scripts/build.sh justin                         # compile production
./scripts/build.sh justin --target upload         # flash via USB
./scripts/build.sh all                             # compile all production projects
cd firmware/justin && pio device monitor   # serial log at 9600 baud
```

`scripts/build-firmware.sh` remains temporarily as a migration wrapper for the documented `-e`, `-t`, and `-v` options; it does not forward arbitrary PlatformIO options.

### Wokwi simulator

Simulate the ESP32 + 4-module MAX7219 matrix without hardware.

**Requirements:** [PlatformIO](https://platformio.org/) and the [Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode) extension.

1. Keep the repository root open as your Cursor workspace.
2. Build the simulator environment: `./scripts/build.sh justin --env wokwi`
3. Run `Cmd+Shift+P` → **Wokwi: Select Config File**, then choose
   `firmware/justin/wokwi.toml`.
4. Run `Cmd+Shift+P` → **Wokwi: Start Simulator**.
5. Keep the **simulator tab visible** — Wokwi pauses when you switch away.
6. Open **`http://localhost:8180`** (not `https://`) in your browser to access the simulated setup portal.
7. On first boot the firmware auto-connects to **`Wokwi-GUEST`** for simulator setup.

The `wokwi` PlatformIO environment defines `WOKWI_SIM=1`, so the firmware tries `Wokwi-GUEST` before starting the normal setup hotspot. Hardware builds use the default `esp32dev` environment.

If `localhost:8180` does not load:

- Run **Wokwi: Select Config File** again and confirm
  `firmware/justin/wokwi.toml` is selected.
- Stop and restart the simulator after changing `wokwi.toml`.
- Check the serial log for `Wokwi setup portal ready.` and `Open http://localhost:8180`.
- If you see `Setup AP` instead, the sim did not join `Wokwi-GUEST`; reset the ESP32 in the simulator and try again.

### Web installer binaries

The [browser installer](https://justinmahar.github.io/esp32-tinker/) uses pre-built flash images in `docs/`, referenced by `docs/manifest.json`:

| File                         | Source (after `pio run`)                          |
| ---------------------------- | ------------------------------------------------- |
| `docs/bootloader.bin`        | `firmware/justin/.pio/build/esp32dev/bootloader.bin` |
| `docs/partitions.bin`        | `firmware/justin/.pio/build/esp32dev/partitions.bin` |
| `docs/firmware_VERSION.bin`  | `firmware/justin/.pio/build/esp32dev/firmware.bin`   |

To refresh the web installer after firmware changes:

```bash
./scripts/bump-version.sh justin
./scripts/build.sh justin
./scripts/publish.sh justin
```

`scripts/publish.sh` reads the registered production environment and version, copies its artifacts, and generates the project's installer manifest. `scripts/update-web-installer.sh` remains as a temporary compatibility wrapper for `justin`.

**Note:** OTA updates on a flashed device use the app partition binary only. The browser installer flashes the full image (bootloader + partition table + app).

Preview locally before publishing:

```bash
./scripts/preview-installer.sh
```

---

## Credits

- Forked from the [YouTube Subscriber Counter](https://github.com/ThePrintingPilot/YouTube-Subscriber-Counter) — original project, 3D enclosure, and browser installer design by [**The Printing Pilot**](https://github.com/ThePrintingPilot)
- Number formatting code by [The Swedish Maker](https://www.youtube.com/@TheSwedishMaker)

---

## License

MIT
