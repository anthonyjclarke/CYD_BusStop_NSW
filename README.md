# CYD_BusStop

ESP32 bus stop notifier for the **ESP32-2432S028R (CYD 2.8")** displaying live NSW
bus departures for four stops near Ryde/Putney. Built with PlatformIO and the Arduino
framework.

---

## What It Does

- Fetches the next 3 departures per stop from the TfNSW Trip Planner API
- Displays all four stops simultaneously in a 2×2 grid (landscape 320×240)
- Shows departure as route · minutes-until · clock time (e.g. `501  3m  10:48`)
- Live time and date header, updated every second via NTP
- Web interface at the device IP: state JSON, OTA update, config (Phase 2)

---

## Hardware

| Component  | Detail                               |
|:-----------|:-------------------------------------|
| Board      | ESP32-2432S028R (CYD 2.8")           |
| Display    | ILI9341 · 240×320 · SPI              |
| Touch      | XPT2046 (unused in this project)     |
| Flash      | 4 MB · custom partition table        |
| USB–Serial | CP2102                               |

---

## Prerequisites

### TfNSW API Key

This project uses the **TfNSW Open Data Trip Planner API** (Departure Monitor endpoint).

1. Register at [opendata.transport.nsw.gov.au](https://opendata.transport.nsw.gov.au)
2. Sign in and go to **My Account → Applications → Add Application**
3. Give the application a name (e.g. `CYD BusStop`) and request access to the
   **Trip Planner APIs** product
4. Once approved (usually instant), copy the API key from the application detail page
5. Paste it into `include/secrets.h` as `SECRET_TFNSW_API_KEY` (see [Configuration](#configuration))

The key is sent as an HTTP header: `Authorization: apikey <your-key>`.
Requests are made to:

```
https://api.transport.nsw.gov.au/v1/tp/departure_mon
```

The free tier allows sufficient polling for personal/hobbyist use at the 60 s
interval used here.

### Toolchain

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Libraries are fetched automatically on first build via `lib_deps` in `platformio.ini`

---

## Project Structure

```
CYD_BusStop/
├── platformio.ini             # Board, framework, libs, TFT build flags
├── partitions_custom.csv      # 4 MB flash layout with OTA slots
├── include/
│   ├── config.h               # Stop IDs, API constants, display defaults
│   ├── debug.h                # Leveled DBG_* macros (ERROR/WARN/INFO/VERBOSE)
│   └── secrets.h              # Gitignored — WiFi + API credentials
└── src/
    ├── main.cpp               # setup(), loop(), init orchestration
    ├── display.cpp/.h         # TFT drawing — header, panels, status bar
    ├── bus_api.cpp/.h         # TfNSW API fetch + JSON parse
    ├── time_mgr.cpp/.h        # ezTime NTP init + time/date strings
    └── web_server.cpp/.h      # AsyncWebServer routes + ElegantOTA
```

---

## Configuration

### `include/secrets.h` (gitignored — create before first build)

```cpp
#pragma once
#define SECRET_WIFI_SSID     "your-ssid"
#define SECRET_WIFI_PASS     "your-password"
#define SECRET_TFNSW_API_KEY "your-tfnsw-api-key"
```

### `include/config.h`

Key tuneable constants:

| Constant             | Default           | Purpose                      |
|:---------------------|:------------------|:-----------------------------|
| `POLL_INTERVAL_MS`   | `60000`           | Bus API refresh interval     |
| `BRIGHTNESS_DEFAULT` | `200`             | Backlight (0–255)            |
| `TIME_24HR_DEFAULT`  | `false`           | 12 hr display                |
| `WIFI_AP_NAME`       | `"CYD-BusStop"`   | Captive portal AP name       |
| `OTA_HOSTNAME`       | `"cyd-busstop"`   | mDNS + ArduinoOTA hostname   |

---

## Stop Configuration

Stops are defined in `include/config.h`:

| Stop ID | Display Name    |
|:--------|:----------------|
| 2112130 | To Gladesville  |
| 2112131 | To Meadowbank   |
| 211267  | End of Small St |
| 211271  | To Macquarie    |

To change stops, update `STOP_IDS[]` and `STOP_NAMES[]` in `config.h`.

---

## Building & Flashing

```bash
# Build
pio run

# Flash
pio run --target upload

# Monitor serial output
pio device monitor
```

Upload speed is set to 921600 baud. Port is auto-detected by PlatformIO.

---

## First Boot

1. Power on — the display shows `Connecting WiFi...`
2. Connect to the `CYD-BusStop` AP from your phone or computer
3. Complete the captive portal with your WiFi credentials
4. The device connects, syncs time, fetches buses, and draws the display

If `SECRET_WIFI_SSID` / `SECRET_WIFI_PASS` are set in `secrets.h`, the portal
is pre-filled and the device connects automatically if credentials are valid.

---

## Display Layout

```
┌──────────────────────────────────────┐
│  10:45 AM              Tue, 1 Apr    │
├───────────────────┬──────────────────┤
│ To Gladesville    │ To Meadowbank    │
│  501  3m  10:48   │  501  1m  10:46  │
│  501  13m 10:58   │  501  11m 10:56  │
│  501  23m 11:08   │  501  21m 11:06  │
├───────────────────┼──────────────────┤
│ End of Small St   │ To Macquarie     │
│  500  5m  10:50   │  500  7m  10:52  │
│  500  15m 11:00   │  500  17m 11:02  │
│  500  25m 11:10   │  500  27m 11:12  │
└───────────────────┴──────────────────┘
```

Minutes-until are green (< 10 min) or yellow (≥ 10 min).

---

## Web Interface

| Route        | Purpose                                      |
|:-------------|:---------------------------------------------|
| `/`          | Home — links to all routes                   |
| `/api/state` | JSON — current time + all departure data     |
| `/update`    | ElegantOTA firmware upload page              |
| `/mirror`    | Canvas display mirror *(Phase 3)*            |

Access via the device IP shown in serial output, or `http://cyd-busstop.local/`
if your network supports mDNS.

---

## OTA Updates

Two OTA methods are supported:

**ArduinoOTA** — upload directly from PlatformIO during development:
```bash
pio run --target upload --upload-port cyd-busstop.local
```

**ElegantOTA** — browser-based upload via `/update`. Upload the `.pio/build/cyd/*.bin`
firmware file.

---

## Debug Output

Set `DEBUG_LEVEL` in `platformio.ini` build_flags:

| Level | Macro          | Output                         |
|:------|:---------------|:-------------------------------|
| 1     | `DBG_ERROR`    | Critical failures              |
| 2     | `DBG_WARN`     | Unexpected but recoverable     |
| 3     | `DBG_INFO`     | State changes, init (default)  |
| 4     | `DBG_VERBOSE`  | Frequent events, values        |

---

## Roadmap

- **Phase 1** *(current)*: WiFi · NTP · TfNSW API · display layout
- **Phase 2**: Web config page · NVS persistence · 12/24 hr toggle · brightness control
- **Phase 3**: Canvas display mirror at `/mirror` · font upgrade to VLW NotoSans

---

## Licence

Personal project — not licensed for redistribution.
