# CYD_BusStop_NSW — Project Context

## What It Does

Displays the next 3 bus departures for 4 NSW stops near Ryde/Putney, fetched from the
TfNSW Trip Planner API. Shows a live time/date header on the TFT and a live web
dashboard at `/` with editable stops, delay indicators, real-time badges, and alerts.

## Target Hardware

ESP32-2432S028R (CYD 2.8") — ILI9341, 240x320, no PSRAM.
Standard CYD pin assignments apply — see global CLAUDE.md.

## Stop Configuration

| Stop ID | Display Name     |
|:--------|:-----------------|
| 2112130 | To Gladesville   |
| 2112131 | To Meadowbank Stn |
| 211267  | End of Small St  |
| 211271  | To Macquarie Park |

Defaults defined in `config.h`. At runtime, the active stop list is stored in NVS
and can be edited from the WebUI "Edit stops" pane.

## API

**Provider:** TfNSW Open Data — Trip Planner APIs (Departure Monitor)
**Registration:** opendata.transport.nsw.gov.au — sign in → My Account →
Applications → Add Application → request **Trip Planner APIs** product.
Key available on the application detail page once approved.
**Endpoint:** `https://api.transport.nsw.gov.au/v1/tp/departure_mon`

| Detail        | Value                                 |
|:--------------|:--------------------------------------|
| Auth header   | `Authorization: apikey <key>`         |
| Key constant  | `SECRET_TFNSW_API_KEY` in `secrets.h` |
| Poll interval | 60 s · 4 requests · 500 ms gap        |
| Response size | ~60 KB per stop (chunked transfer)    |
| Parsing       | `DeChunkStream` — zero-copy JSON      |
| Timestamps    | UTC from API; local via `myTZ`        |

Parsed departure fields: route, destination, estimated epoch, planned epoch,
delay (seconds), real-time flag, and optional alert subtitle. Departures are
collected (up to 8), sorted by estimated epoch, and the 3 soonest are kept.

## Module Structure

| File                    | Purpose                                              |
|:------------------------|:-----------------------------------------------------|
| `src/main.cpp`          | setup(), loop(), init orchestration, bus refresh      |
| `src/display.cpp/.h`    | All TFT drawing — header, panels, status bar         |
| `src/bus_api.cpp/.h`    | TfNSW API fetch, JSON parse, sort, StopData structs  |
| `src/time_mgr.cpp/.h`   | ezTime NTP init, time/date/day helpers, TZ offset    |
| `src/web_server.cpp/.h` | AsyncWebServer routes, WebUI, `/api/state`, refresh Q |
| `src/config.cpp`        | Stop config: NVS load/save/reset, runtime arrays     |
| `include/config.h`      | All tuneable constants + stop config declarations    |
| `include/debug.h`       | Levelled debug macros with wall-clock timestamps     |

## Fonts

Currently using TFT_eSPI built-in fonts (requires `-DLOAD_FONT2=1 -DLOAD_FONT4=1`):
- Font 4 (26px) — header time
- Font 2 (16px) — stop names, departure rows, status bar

Upgrade path when ready: generate NotoSansBold VLW fonts via Processing
`Create_Smooth_Font` sketch, place in `src/fonts/`, and swap `drawString(..., 2/4)`
calls in `display.cpp` for `loadFont` / `drawString` / `unloadFont` pattern.

## Display Layout

Landscape 320x240. Header 28px, then 2x2 grid of stop panels.
Each panel: stop name + 3 departure rows.
Each departure row: route, real-time indicator (`●`/`~`), minutes or day
abbreviation (for non-today departures), and clock time.
Footer area shows `upd HH:MM` after successful fetch.

## Refresh Strategy

| What             | Interval | Mechanism                                  |
|:-----------------|:---------|:-------------------------------------------|
| TFT clock header | 1s       | ezTime `events()` tick + `drawHeader()`    |
| TFT stop panels  | 15s      | `recalcMinutes()` from stored epoch + draw |
| API fetch        | 60s      | `fetchAllStops()` — 4 HTTPS requests       |
| WebUI render     | 5s       | Client-side JS recalc from epoch           |
| WebUI API poll   | 15s      | `fetch('/api/state')` from browser         |
| WebUI stop edit  | on-demand| `consumeStopRefreshRequest()` in loop()    |

## Web API Endpoints

| Method | Path               | Description                                          |
|:-------|:-------------------|:-----------------------------------------------------|
| GET    | `/`                | Live web dashboard + stop editor                     |
| GET    | `/api/state`       | JSON: time, date, UTC epoch, TZ offset, stop data    |
| GET    | `/api/stops`       | JSON array of current stop id/name pairs             |
| POST   | `/api/stops`       | Update stop list (JSON array); persists + queues refresh |
| POST   | `/api/stops/reset` | Restore default stops; persists + queues refresh     |
| GET    | `/mirror`          | Redirects to `/`                                     |

`/api/state` fields: `time`, `date`, `now` (UTC epoch), `tzOff` (seconds),
`stops[]` with optional `alert`, and per-departure `route`, `clock`, `minutes`,
`epoch`, `rt` (bool), `delay` (seconds), and optional `dest`.

## Build Phases

- **Phase 1** ✓: WiFi + NTP + API fetch + display + web dashboard + OTA
- **Phase 2** ✓ (partial): NVS stop config persistence + web stop editor UI +
  richer departure metadata (RT, delay, destination, alerts, day labels)
- **Phase 2** (remaining): Full `/config` page (poll interval, brightness, timezone)
- **Phase 3**: Canvas display mirror at `/mirror` using `/api/state` JSON

## Known Issues / Notes

- CYD has no PSRAM — no framebuffer mirror possible; web mirror will use JS canvas
- `setInsecure()` used on WiFiClientSecure — acceptable for device, not prod cloud
- `mktime()` on ESP32 Arduino treats struct tm as UTC — `parseISODatetime()` relies on this
- TfNSW API ignores `&limit=6` parameter — always returns full result set (~60KB)
- `DeChunkStream` handles the chunked responses with zero heap buffer overhead
- `DeChunkStream::_timedRead()` still uses `delay(1)` — should be `yield()` (tracked)
- Built-in fonts require explicit `-DLOAD_FONT2=1` / `-DLOAD_FONT4=1` in build flags
- `fetchAllStops()` blocks the loop for ~10s (4 HTTPS requests with TLS) — OTA
  and web requests are unresponsive during this window; FreeRTOS task planned
- Stop config web editor has no client-side validation — server rejects invalid
  input (length checks) but no UI feedback yet; tracked in CHANGELOG enhancements
- `TIME_24HR_DEFAULT = false` → header displays 12hr format (e.g. "2:35 PM");
  set to `true` in `config.h` for 24hr ("14:35")
- WebUI stop edit uses `consumeStopRefreshRequest()` to defer fetch to the main
  loop — keeps the async web handler non-blocking
- TfNSW `occupancy` field is not populated on Ryde/Putney routes — confirmed absent
  via diagnostic fetch; not worth implementing

## Flashing

Port: auto-detected by PlatformIO. No special flags needed.
After WiFiManager provisioning, device hostname: `cyd-busstop` (mDNS + OTA).
