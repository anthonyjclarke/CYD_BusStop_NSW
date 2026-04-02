# Changelog

All notable changes to this project will be documented here.
Format: `## [version] YYYY-MM-DD` — sections: Added · Changed · Fixed.

---

## [0.2.3] 2026-04-02

### Added
- Runtime-configurable stop list via WebUI:
  - `initStopConfig()` loads persisted stops from NVS (`Preferences`) or falls
    back to built-in defaults.
  - `stopIds[]/stopNames[]` runtime arrays replace hardcoded `STOP_IDS`/`STOP_NAMES`
    for fetch/display and API APIs.
  - `GET /api/stops` returns the current stop id/name array.
  - `POST /api/stops` accepts JSON array to update the stop list (up to
    `STOP_COUNT` entries), persists with `saveStopConfig()`, and triggers
    `fetchAllStops()`.
  - `POST /api/stops/reset` restores defaults and refreshes data.
- Minimal WebUI stop editor on `/`:
  - “Edit stops” toggle reveals editable stop ID/name fields.
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
