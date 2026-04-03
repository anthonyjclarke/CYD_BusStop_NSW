# Changelog

All notable changes to this project will be documented here.
Format: `## [version] YYYY-MM-DD` — sections: Added · Changed · Fixed.

---

## Things to do / Enhancements

- Add client-side validation for stop ID (numeric only, max length) and stop
  name length before submitting.
- Support adding/removing stops dynamically (up to `STOP_COUNT`) in the UI and
  potentially in runtime config.
- Add UI feedback on validation errors in the editor form and disable save when
  invalid.
- Add secure admin access to web config endpoints (password / token) to prevent
  unauthorized changes.
- Add optional zone and stop name autocomplete using TfNSW lookup endpoint.
- Add a persistent JSON file fallback (LittleFS) in addition to NVS for config
  portability and ease of manual editing.
- Add a dedicated `/config` page in WebUI for full device settings (poll
  interval, display brightness, timezone, etc.)
- Add full config page in WebUI, including system diagnostics.

### Code optimisations

- Replace `delay(1)` with `yield()` in `DeChunkStream::_timedRead()` — keeps
  the WiFi/lwIP stack alive during byte-by-byte stream reads; `delay(1)`
  blocks FreeRTOS for a full tick on every byte of a ~60KB response.
- Replace `DynamicJsonDocument(8192)` in `fetchStop()` with
  `StaticJsonDocument<4096>` — the ArduinoJson filter limits the parsed tree;
  heap-allocating and freeing 8KB four times per poll cycle causes fragmentation.
- Fix vertical divider overdraw in `drawDividers()`: `drawFastVLine(PANEL_W,
  HEADER_H, 240, ...)` draws 28px past the bottom of the 240px screen —
  length should be `240 - HEADER_H` (212).
- Replace `DynamicJsonDocument` in `handleApiState()` and `handleApiStops()`
  with `StaticJsonDocument` equivalents — both are called on every browser poll.
- Replace heap-allocated `String` returns from `stopIdKey()` / `stopNameKey()`
  in `config.cpp` with stack `char` buffers via `snprintf`.
- Fix double-`String` construction in `bus_api.cpp` fetch log line:
  `String(String(contentLen) + " bytes")` allocates two objects; use `%d`
  format directly.
- Move `fetchAllStops()` to a FreeRTOS task (Phase 3) — currently blocks the
  main loop for ~10s, leaving OTA and web requests unresponsive.

---

## [0.2.4] 2026-04-03

### Added
- TfNSW departure parsing now captures:
  - real-time control flag (`isRealtimeControlled`)
  - planned vs estimated departure epochs and delay in seconds
  - destination name (e.g. "Gladesville - Jordan St")
  - first service alert subtitle per stop
- TFT real-time indicators per departure row:
  - green filled circle (`●`) for GPS-tracked live departures
  - grey tilde (`~`) for scheduled-only departures
- TFT and WebUI day abbreviation (Mon, Tue, etc.) for non-today departures
  instead of showing large minute counts
- WebUI per-departure `LIVE`/`SCHED` badge based on real-time status
- WebUI delay pill: `+4m late` (orange) or `3m early` (green), suppressed
  below ±2 min to filter GPS noise
- WebUI destination column per departure row
- WebUI amber alert banner when TfNSW returns service disruption info
- `isLocalToday()`, `formatLocalDayAbbr()`, `getLocalTZOffset()`,
  `getLogTimeStr()` helpers in `time_mgr`
- `/api/state` now includes `tzOff` (TZ offset in seconds) for client-side
  local-day calculation
- Wall-clock timestamps in debug log output (local HH:MM:SS after NTP sync,
  uptime fallback before sync)
- `maxBlk` (largest contiguous free heap block) in fetch and init log lines
- `performBusRefresh()` helper in `main.cpp` to deduplicate fetch+draw logic
- `consumeStopRefreshRequest()` mechanism so WebUI stop edits queue a refresh
  back to the main loop instead of fetching inside the async handler

### Changed
- "Now" departure colour changed from red to orange on both TFT and WebUI for
  better visual distinction from "Gone" (red, WebUI only)
- WebUI poll interval reduced from 60s to 15s; render interval from 15s to 5s
  — WebUI now stays closer to TFT update cadence
- `fetchStop()` now collects up to 8 departures, sorts by estimated epoch, and
  keeps the 3 soonest — fixes out-of-order display when buses run early/late
- `/api/state` response includes `rt`, `delay`, `dest` per departure and
  `alert` per stop
- WebUI layout: departure rows now have distinct columns for route, badge,
  destination, delay pill, minutes/day label, and clock time
- WebUI stop edit POST now queues a refresh instead of calling `fetchAllStops()`
  directly in the async handler (non-blocking)
- WebUI stop edit POST logs individual field changes and detects no-op saves
- Delay logged in minutes (`delay:+4m`) instead of seconds
- Default stop names updated: "To Meadowbank Stn", "To Macquarie Park"
- Debug macros use `dbgTimestamp()` with wall-clock output via `getLogTimeStr()`

### Removed
- Temporary diagnostic fetch (`DIAG_FETCH`) code and build flag

---

## [0.2.3] 2026-04-02

### Added
- Runtime-configurable stop list via WebUI:
  - `initStopConfig()` loads persisted stops from NVS (`Preferences`) or falls
    back to built-in defaults.
  - `stopIds[]/stopNames[]` runtime arrays replace hardcoded `STOP_IDS`/`STOP_NAMES`
    for fetch/display and API references.
  - `GET /api/stops` returns the current stop id/name array.
  - `POST /api/stops` accepts JSON array to update the stop list (up to
    `STOP_COUNT` entries), persists with `saveStopConfig()`, and triggers
    `fetchAllStops()`.
  - `POST /api/stops/reset` restores defaults and refreshes data.
- Minimal WebUI stop editor on `/`:
  - "Edit stops" toggle reveals editable stop ID/name fields.
  - Save and reset buttons with status messages.
  - Client JS syncs with API and updates the live stops display.

## [0.2.2] 2026-04-02

### Added
- "upd HH:MM" last-fetch indicator — dim grey, bottom-right corner, Font 2.
  Sits in the 16 px gap (y=224–239) below the last departure row. Redrawn
  after every `drawAllStops()` call; only changes value on actual API fetch.

---

## [0.2.1] 2026-04-02

### Fixed
- Bus departure clock times displayed in UTC instead of local Sydney time.
  TfNSW API returns timestamps in UTC; `clockTime` was extracted directly from
  the raw ISO string (`dtStr + 11`) giving UTC HH:MM. Fix: derive `clockTime`
  via `formatLocalHHMM()` which uses ezTime's `myTZ.dateTime(epoch, UTC_TIME)`
  to convert the stored UTC epoch to AEDT/AEST before display.

---

## [0.2.0] 2026-04-01

### Added
- De-chunking stream wrapper (`DeChunkStream`) for reliable TfNSW API parsing
  — handles chunked transfer-encoding that the API always sends, regardless of
  HTTP/1.0 request; zero heap buffer allocation
- Live countdown: `recalcMinutes()` recalculates minutes-until from stored
  departure epoch every 15s on TFT and client-side in WebUI
- "Now" label (red) when a bus is 0 minutes away on both TFT and WebUI
- "Gone" label (red) in WebUI for departed buses
- WebUI live dashboard at `/` — dark theme, polls `/api/state` every 60s,
  JS recalculates minutes every 15s, shows "Updated Xs ago"
- `/api/state` now includes `now` (UTC epoch), `fetchAge` per stop, and
  `epoch` per departure for client-side recalculation
- `&limit=6` added to TfNSW API query string to request fewer results

### Changed
- Replaced ElegantOTA with ArduinoOTA (framework built-in, no extra library)
- Replaced `me-no-dev/ESP Async WebServer` + `AsyncTCP` with maintained
  `mathieucarbou/ESPAsyncWebServer` + `AsyncTCP` via GitHub URL refs
- Added `-DLOAD_GLCD=1`, `-DLOAD_FONT2=1`, `-DLOAD_FONT4=1` build flags
  — built-in fonts were not compiled without these, causing blank TFT text
- `NTP_SERVER` constant renamed to `NTP_HOST` to avoid macro collision with
  ezTime's `#define NTP_SERVER`
- `Departure` struct now stores `epochUTC` for live recalculation
- WebUI root page changed from server-rendered HTML to PROGMEM + JS polling

### Fixed
- TFT display showing grid lines but no text (missing LOAD_FONT build flags)
- Heap fragmentation causing stops 3–4 to fail with `IncompleteInput`
  (replaced large `getString()` buffer with streaming de-chunk parser)
- `fetchAllStops()` printf missing `STOP_COUNT` argument — printed stack garbage
- Duplicate ArduinoOTA initialisation (was in both `main.cpp` and `web_server.cpp`)

---

## [0.1.0] 2026-04-01

### Added
- Initial project scaffold — PlatformIO, Arduino framework, CYD 2.8" target
- `platformio.ini` with full TFT_eSPI CYD build flags and custom partition table
- Modular source layout: `display`, `bus_api`, `time_mgr`, `web_server`
- TfNSW Trip Planner API integration (`/v1/tp/departure_mon`)
  — ArduinoJson filter to minimise heap usage per fetch
  — ISO 8601 datetime parser with UTC offset handling for minutes-until calculation
- WiFiManager captive portal provisioning with optional `secrets.h` credential seed
- ezTime NTP sync (`Australia/Sydney` timezone)
- 2x2 stop panel display layout (landscape 320x240)
  — Departure rows: route · minutes-until (colour-coded) · clock time
  — Font 4 (26 px) for time header, Font 2 (16 px) for all body text
- ArduinoOTA web firmware upload
- AsyncWebServer with `/api/state` JSON endpoint, `/mirror` placeholder
- `README.md` with setup, layout, web interface, and OTA documentation
