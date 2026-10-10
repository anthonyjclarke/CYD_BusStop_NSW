# Web installer

The browser installer at
[anthonyjclarke.github.io/CYD_BusStop_NSW](https://anthonyjclarke.github.io/CYD_BusStop_NSW/)
is built by the shared [cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer)
workflow (`.github/workflows/firmware.yml`). Release images come only from CI
on a `v*` tag on `main`, so they never contain `include/secrets.h`.

---

## Project specifics

| Item                | Value                                       |
|:--------------------|:--------------------------------------------|
| Envs in installer   | `cyd` – CYD 2.8″ ESP32-2432S028R            |
| `PROJECT_NAME`      | `CYD_BusStop_NSW` (frozen)                  |
| Partitions          | `partitions_custom.csv`, dual-OTA (frozen)  |
| App slot / image    | 1,835,008 B / ~1,223,000 B (66 %)           |
| Improv device name  | `BusStop-XXXX` (MAC's last two bytes)       |
| Setup hotspot       | `CYD-BusStop`                               |
| OTA                 | ArduinoOTA (`cyd-busstop`) – `app1` applies |
| User must bring     | Own TfNSW Trip Planner API key              |

The TfNSW key is not in the image. Users enter it on `/config` or in the
hotspot's portal; it is stored in NVS (`busstop/apikey`) and an Update keeps
it. Improv is ticked during the ~10 s bus fetch, except inside each TLS
handshake (~1–2 s per stop), so a Connect that lands there can offer Install –
retrying fixes it.

---

## Smoke test (RUNBOOK 5a) – passed 10-10-2026

| Item     | Detail                                                    |
|:---------|:----------------------------------------------------------|
| Board    | CYD 2.8″, ESP32-D0WD-V3 rev 3.1, CH340, `b0:cb:d8:da:ae:8c` |
| Image    | CI preview, Firmware run 38013937097 (`04f0e6b`), `0.4.0-dev` |
| Browser  | Chrome on macOS, preview served from `localhost:8000`     |

1. CI green for the only env (`cyd`).
2. Erased with `pio run -t erase`, fresh install from the preview with erase,
   then **Configure WiFi** over Improv (device `BusStop-AE8C`). The board
   joined WiFi, synced NTP (AEDT, local POSIX rule) and showed
   `No API key - <ip>/config` on the TFT.
3. **Visit device** → dashboard banner → `/config`; key saved and departures
   loaded for all four stops.
4. Dialog closed, **Connect** again: "Connected to CYD_BusStop_NSW
   0.4.0-dev" – Improv answered, so Update will be offered.
5. Boot log after reset: `Running from app0`, `API key: stored`, 4 × 8
   departures, free heap 209,256 B, no crash. The key appears nowhere in the
   serial output.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [x] Case 1 – fresh install, erased (only board env) – 10-10-2026, `b0:cb:d8:da:ae:8c`
- [x] Case 2 – Update on a provisioned board – 10-10-2026, `b0:cb:d8:da:ae:8c`:
  live page offered Update from `0.4.0-dev`, no erase; booted `v0.4.0` from
  `app0` with stops, settings, API key and WiFi kept
- [ ] Case 3 – Update from `app1` after ArduinoOTA

Cases 4 (wrong board) and 5 (web `/update`) don't apply: one env, and no web
update page.
