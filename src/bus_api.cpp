#include "bus_api.h"
#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"
#include "../include/secrets.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

StopData stopData[STOP_COUNT];

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Parse an ISO 8601 datetime string ("2024-03-15T10:48:00+11:00") to UTC epoch.
// mktime() on ESP32 Arduino treats struct tm as UTC, so we subtract the
// offset embedded in the string to arrive at UTC.
static time_t parseISODatetime(const char* s) {
  int  Y, M, D, h, m, sec;
  int  tzh = 0, tzm = 0;
  char sign = '+';

  int parsed = sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%c%2d:%2d",
                      &Y, &M, &D, &h, &m, &sec, &sign, &tzh, &tzm);

  if (parsed < 6) {
    DBG_WARN("parseISODatetime: bad format: %s", s);
    return 0;
  }

  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon  = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min  = m;
  t.tm_sec  = sec;

  time_t epoch      = mktime(&t);  // treated as UTC on ESP32 Arduino
  int    offsetSecs = tzh * 3600 + tzm * 60;

  return (sign == '-') ? epoch + offsetSecs : epoch - offsetSecs;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initBusApi() {
  memset(stopData, 0, sizeof(stopData));
  DBG_INFO("Bus API initialised — %d stops configured", STOP_COUNT);
}

bool fetchStop(uint8_t idx) {
  if (idx >= STOP_COUNT) return false;

  if (WiFi.status() != WL_CONNECTED) {
    DBG_WARN("fetchStop[%d]: WiFi not connected", idx);
    return false;
  }

  // Build URL
  String url = TFNSW_API_BASE;
  url += "&name_dm=";
  url += STOP_IDS[idx];

  // Filter to only extract the fields we need — reduces DynamicJsonDocument size
  StaticJsonDocument<192> filter;
  filter["stopEvents"][0]["departureTimePlanned"]  = true;
  filter["stopEvents"][0]["departureTimeEstimated"] = true;
  filter["stopEvents"][0]["transportation"]["number"] = true;

  WiFiClientSecure client;
  client.setInsecure();  // no cert validation — acceptable for this use case

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Authorization", "apikey " SECRET_TFNSW_API_KEY);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DBG_WARN("fetchStop[%d] %s HTTP %d", idx, STOP_IDS[idx], code);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(
    doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    DBG_ERROR("fetchStop[%d] JSON: %s", idx, err.c_str());
    return false;
  }

  // Parse stop events
  JsonArray events = doc["stopEvents"].as<JsonArray>();
  if (events.isNull()) {
    DBG_WARN("fetchStop[%d]: no stopEvents in response", idx);
    return false;
  }

  time_t  nowUTC = getUTCNow();
  uint8_t count  = 0;

  // Zero out existing data before repopulating
  memset(&stopData[idx], 0, sizeof(StopData));

  for (JsonObject ev : events) {
    if (count >= DEPARTURES_PER_STOP) break;

    // Prefer real-time estimated time, fall back to planned
    const char* dtStr = ev["departureTimeEstimated"] | ev["departureTimePlanned"];
    if (!dtStr) continue;

    time_t depEpoch = parseISODatetime(dtStr);
    if (depEpoch == 0) continue;

    int mins = (int)((depEpoch - nowUTC) / 60);
    if (mins < 0) continue;  // already departed

    Departure& dep   = stopData[idx].departures[count];
    dep.minutesUntil = mins;

    // Clock time is chars 11–15 of the ISO string (already local time)
    strncpy(dep.clockTime, dtStr + 11, 5);
    dep.clockTime[5] = '\0';

    const char* routeNum = ev["transportation"]["number"];
    if (routeNum) {
      strncpy(dep.route, routeNum, sizeof(dep.route) - 1);
    }

    dep.valid = true;
    count++;
  }

  stopData[idx].count       = count;
  stopData[idx].valid       = (count > 0);
  stopData[idx].lastFetchMs = millis();

  DBG_INFO("Stop %-12s — %d departure(s)", STOP_IDS[idx], count);
  return stopData[idx].valid;
}

void fetchAllStops() {
  DBG_INFO("Fetching all %d stops...", STOP_COUNT);
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    fetchStop(i);
    if (i < STOP_COUNT - 1) {
      delay(INTER_REQUEST_MS);  // small gap — avoids hammering the API
    }
  }
}
