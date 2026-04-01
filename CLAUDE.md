# CYD_BusStop — Project Context

## What It Does

Displays the next 3 bus departures for 4 NSW stops near Ryde/Putney, fetched from the
TfNSW Trip Planner API. Shows a live time/date header. Live web dashboard at `/`.

## Target Hardware

ESP32-2432S028R (CYD 2.8") — ILI9341, 240x320, no PSRAM.
Standard CYD pin assignments apply — see global CLAUDE.md.

## Stop Configuration

| Stop ID | Display Name    |
|:--------|:----------------|
| 2112130 | To Gladesville  |
| 2112131 | To Meadowbank   |
| 211267  | End of Small St |
| 211271  | To Macquarie    |

## API

- Endpoint: `https://api.transport.nsw.gov.au/v1/tp/departure_mon`
- Auth header: `Authorization: apikey <key>`
- Key stored in `include/secrets.h` as `SECRET_TFNSW_API_KEY` (gitignored)
- Poll interval: 60s, 4 sequential requests per cycle (500ms gap between requests)
- `&limit=6` in query string — API ignores this, responses are still ~60KB each
- Responses always use chunked transfer-encoding; `DeChunkStream` in `bus_api.cpp`
  strips framing so ArduinoJson can stream-parse directly (zero large buffer)

## Module Structure

| File                    | Purpose                                       |
|:------------------------|:----------------------------------------------|
| `src/main.cpp`          | setup(), loop(), init orchestration            |
| `src/display.cpp/.h`    | All TFT drawing — header, panels, status bar   |
| `src/bus_api.cpp/.h`    | TfNSW API fetch, JSON parse, StopData structs  |
| `src/time_mgr.cpp/.h`   | ezTime NTP init, time/date string helpers      |
| `src/web_server.cpp/.h` | AsyncWebServer routes, ArduinoOTA, /api/state  |

## Fonts

Currently using TFT_eSPI built-in fonts (requires `-DLOAD_FONT2=1 -DLOAD_FONT4=1`):
- Font 4 (26px) — header time
- Font 2 (16px) — stop names, departure rows, status bar

Upgrade path when ready: generate NotoSansBold VLW fonts via Processing
`Create_Smooth_Font` sketch, place in `src/fonts/`, and swap `drawString(..., 2/4)`
calls in `display.cpp` for `loadFont` / `drawString` / `unloadFont` pattern.

## Display Layout

Landscape 320x240. Header 28px, then 2x2 grid of stop panels.
Each panel: stop name + 3 departure rows (route · Xm/Now · HH:MM).

## Refresh Strategy

| What             | Interval | Mechanism                                  |
|:-----------------|:---------|:-------------------------------------------|
| TFT clock header | 1s       | ezTime `events()` tick + `drawHeader()`    |
| TFT stop panels  | 15s      | `recalcMinutes()` from stored epoch + draw |
| API fetch        | 60s      | `fetchAllStops()` — 4 HTTPS requests       |
| WebUI minutes    | 15s      | Client-side JS recalc from epoch           |
| WebUI API poll   | 60s      | `fetch('/api/state')` from browser         |

## Build Phases

- **Phase 1 (current)**: WiFi + NTP + API fetch + display + web dashboard + OTA
- **Phase 2**: Web config page, NVS persistence
- **Phase 3**: Canvas display mirror at `/mirror` using `/api/state` JSON

## Known Issues / Notes

- CYD has no PSRAM — no framebuffer mirror possible; web mirror will use JS canvas
- `setInsecure()` used on WiFiClientSecure — acceptable for device, not prod cloud
- `mktime()` on ESP32 Arduino treats struct tm as UTC — `parseISODatetime()` relies on this
- TfNSW API ignores `&limit=6` parameter — always returns full result set (~60KB)
- `DeChunkStream` handles the chunked responses with zero heap buffer overhead
- Built-in fonts require explicit `-DLOAD_FONT2=1` / `-DLOAD_FONT4=1` in build flags

## Flashing

Port: auto-detected by PlatformIO. No special flags needed.
After WiFiManager provisioning, device hostname: `cyd-busstop` (mDNS + OTA).
