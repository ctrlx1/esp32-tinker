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

Circuit wiring is defined in `Firmware-PIO/diagram.json` (DIN→GPIO23, CLK→GPIO18, CS→GPIO5, power via `V+` / `GND.2`).

---

## First-time setup

### 1. Flash the firmware

**Option A — Browser installer (recommended)**

No IDE needed. Connect your ESP32 via USB and hit Install:

➡ **[Install via browser](https://justinmahar.github.io/esp32-tinker/)**

Requires Chrome or Edge on desktop.

**Option B — PlatformIO**

1. Open the `Firmware-PIO` folder in VS Code or Cursor (PlatformIO extension required)
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

1. Build with PlatformIO — the OTA `.bin` is at `Firmware-PIO/.pio/build/esp32dev/firmware.bin`
2. Open the device IP in your browser
3. Scroll to **Firmware update**, pick the main firmware `.bin`, click **Upload firmware**
4. The matrix shows `OTA...` then `Rebooting` — done

---

## Development

All firmware source lives in `Firmware-PIO/`. Open that folder as your Cursor/VS Code workspace when building, flashing, or simulating.

The project version lives in `VERSION`. Increment it with:

```bash
./scripts/bump-version.sh           # patch bump
./scripts/bump-version.sh --minor   # minor bump
./scripts/bump-version.sh --major   # major bump
```

### Build and flash (PlatformIO)

```bash
./scripts/build-firmware.sh              # compile
./scripts/build-firmware.sh -t upload    # flash via USB
cd Firmware-PIO && pio device monitor      # serial log at 9600 baud
```

### Wokwi simulator

Simulate the ESP32 + 4-module MAX7219 matrix without hardware.

**Requirements:** [PlatformIO](https://platformio.org/) and the [Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode) extension.

1. Open **`Firmware-PIO`** as your workspace root (where `wokwi.toml` lives). If the workspace root is the repo folder instead, Wokwi will not load port forwarding.
2. Build the simulator environment: `./scripts/build-firmware.sh -e wokwi` or `pio run -e wokwi`
3. Start: `Cmd+Shift+P` → **Wokwi: Start Simulator**
4. Keep the **simulator tab visible** — Wokwi pauses when you switch away.
5. Open **`http://localhost:8180`** (not `https://`) in your browser to access the simulated setup portal.
6. On first boot the firmware auto-connects to **`Wokwi-GUEST`** for simulator setup.

The `wokwi` PlatformIO environment defines `WOKWI_SIM=1`, so the firmware tries `Wokwi-GUEST` before starting the normal setup hotspot. Hardware builds use the default `esp32dev` environment.

If `localhost:8180` does not load:

- Confirm the workspace root is **`Firmware-PIO`**, not the parent repo folder.
- Stop and restart the simulator after changing `wokwi.toml`.
- Check the serial log for `Wokwi setup portal ready.` and `Open http://localhost:8180`.
- If you see `Setup AP` instead, the sim did not join `Wokwi-GUEST`; reset the ESP32 in the simulator and try again.

### Web installer binaries

The [browser installer](https://justinmahar.github.io/esp32-tinker/) uses pre-built flash images in `docs/`, referenced by `docs/manifest.json`:

| File                         | Source (after `pio run`)                          |
| ---------------------------- | ------------------------------------------------- |
| `docs/bootloader.bin`        | `Firmware-PIO/.pio/build/esp32dev/bootloader.bin` |
| `docs/partitions.bin`        | `Firmware-PIO/.pio/build/esp32dev/partitions.bin` |
| `docs/firmware_VERSION.bin`  | `Firmware-PIO/.pio/build/esp32dev/firmware.bin`   |

To refresh the web installer after firmware changes:

```bash
./scripts/bump-version.sh
./scripts/build-firmware.sh
./scripts/update-web-installer.sh
```

`scripts/update-web-installer.sh` reads `VERSION`, copies the app binary to `docs/firmware_VERSION.bin`, and rewrites `docs/manifest.json` to reference that versioned firmware file. Commit `VERSION`, `docs/manifest.json`, and the updated `docs/*.bin` files, then push so GitHub Pages serves the new build.

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
