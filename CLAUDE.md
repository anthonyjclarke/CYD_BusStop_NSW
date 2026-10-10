# CYD_BusStop_NSW — Project Context

ESP32-2432S028R (CYD 2.8") — ILI9341 320x240 landscape, no PSRAM. Standard CYD
pins (global CLAUDE.md). Shows the next departures for 4 NSW bus stops from the
TfNSW Trip Planner API (Departure Monitor) on the TFT, plus a web dashboard at
`/` and settings at `/config`. Version in `include/config.h` (`FIRMWARE_VERSION`).

## TfNSW API key

- Never compiled in. Entered in the WiFiManager portal (password field) or on
  `/config` (`POST /api/apikey`), stored in NVS. Never log it, return it from
  an endpoint, or pre-fill a form with it — status only (`apiKeySet`, `apiKey`).
- `include/secrets.h` is optional (`__has_include`); a local
  `SECRET_TFNSW_API_KEY` only seeds NVS once on a board with no key.
- No key → `fetchAllStops()` returns early; 401/403 → "API key rejected".
- Auth header `Authorization: apikey <key>`; `setApiKey()` rejects spaces and
  control chars because the value goes into that header.

## NVS (`busstop` namespace)

| Key                         | Holds                             |
|:----------------------------|:----------------------------------|
| `count`, `defsSig`          | Stop count + hash of defaults     |
| `id0`–`id3`, `name0`–`name3`| Runtime stop list                 |
| `bright`, `time24`, `webdeps` | Display / WebUI settings        |
| `apikey`                    | TfNSW API key                     |

Changing `STOP_IDS_DEFAULT` / `STOP_NAMES_DEFAULT` changes `defsSig`, which
resets saved stops to the new defaults on next boot.

## Gotchas

- TfNSW always replies chunked, even to HTTP/1.0; `DeChunkStream` strips the
  framing so ArduinoJson can parse from the stream with a filter (~60 KB per
  stop). The API ignores `&limit=6`.
- `mktime()` on ESP32 Arduino treats `struct tm` as UTC — `parseISODatetime()`
  relies on this. API times are UTC; local via `myTZ`.
- Timezone uses `setPosix(TIMEZONE_POSIX)`. Never rely on ezTime
  `setLocation()` — it often times out on the CYD and leaves the clock on UTC.
- `fetchAllStops()` blocks ~10 s (4 TLS requests). Web handlers only queue work
  (`consumeStopRefreshRequest()` / `consumeDisplayRefreshRequest()`) — never
  fetch from an async handler.
- Built-in fonts 2/4 need `-DLOAD_FONT2=1 -DLOAD_FONT4=1` in build flags.
- `occupancy` is not populated on these routes — confirmed, don't implement.

## Web installer and releases

- Release images come only from CI on a `v*` tag (`.github/workflows/firmware.yml`
  → cyd-web-installer). Never publish a local build: it contains `secrets.h`.
- Never put `firmware-merged.bin` in a manifest — it wipes NVS.
- `PROJECT_NAME` and `partitions_custom.csv` are frozen; platform pinned to
  `espressif32@6.12.0`.
- Never put Improv back in `lib_deps`; `lib/ImprovWiFi` and
  `src/network/improv_setup.*` are copy-ins — re-copy, don't edit.
- `improvTick()` must run at least every ~1 s: it is called in `loop()`, the
  portal loop, the inter-stop gap and `DeChunkStream::_timedRead()`. Keep it
  there when touching the fetch code.
- Before the next release, clear *Tests owed* in `docs/WEB_INSTALLER.md`
  (RUNBOOK 5b).
