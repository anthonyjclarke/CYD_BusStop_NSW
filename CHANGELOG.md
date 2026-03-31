# Changelog

All notable changes to this project will be documented here.
Format: `## [version] YYYY-MM-DD` — sections: Added · Changed · Fixed.

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
- 2×2 stop panel display layout (landscape 320×240)
  — Departure rows: route · minutes-until (colour-coded) · clock time
  — Font 4 (26 px) for time header, Font 2 (16 px) for all body text
- ArduinoOTA + ElegantOTA web firmware upload (`/update`)
- AsyncWebServer with `/api/state` JSON endpoint, `/mirror` placeholder
- `README.md` with setup, layout, web interface, and OTA documentation
