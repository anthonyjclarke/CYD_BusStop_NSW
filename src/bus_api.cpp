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
// De-chunking stream wrapper
// ---------------------------------------------------------------------------
// The TfNSW API always responds with chunked transfer-encoding (even with
// HTTP/1.0). getStream() returns raw chunks with hex framing — unusable by
// ArduinoJson.  This wrapper strips the chunk framing so the consumer sees
// a clean byte stream.  Memory cost: ~20 bytes on the stack.

class DeChunkStream : public Stream {
  Stream& _src;
  size_t  _remain;   // bytes left in the current chunk
  bool    _eof;

  bool _nextChunk() {
    // Each chunk: <hex-size>\r\n<data>\r\n
    // Final chunk: 0\r\n\r\n
    char buf[12];
    uint8_t i = 0;
    while (i < sizeof(buf) - 1) {
      int c = _timedRead();
      if (c < 0)   { _eof = true; return false; }
      if (c == '\r') { _timedRead(); break; }   // consume \n
      buf[i++] = (char)c;
    }
    buf[i] = '\0';
    _remain = strtoul(buf, nullptr, 16);
    if (_remain == 0) { _eof = true; return false; }
    return true;
  }

  int _timedRead() {
    unsigned long start = millis();
    while (!_src.available()) {
      if (millis() - start > 10000) return -1;
      delay(1);
    }
    return _src.read();
  }

public:
  DeChunkStream(Stream& src) : _src(src), _remain(0), _eof(false) {}

  int available() override { return _eof ? 0 : (_remain ? _remain : 1); }

  int read() override {
    if (_eof) return -1;
    if (_remain == 0 && !_nextChunk()) return -1;
    int c = _timedRead();
    if (c < 0) { _eof = true; return -1; }
    _remain--;
    if (_remain == 0) {
      _timedRead();  // consume \r after chunk body
      _timedRead();  // consume \n
    }
    return c;
  }

  int    peek()             override { return _src.peek(); }
  size_t write(uint8_t)     override { return 0; }
  void   flush()            override {}
};

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

  // Filter — ArduinoJson reads the full stream but only stores these fields.
  StaticJsonDocument<384> filter;
  filter["stopEvents"][0]["departureTimePlanned"]   = true;
  filter["stopEvents"][0]["departureTimeEstimated"] = true;
  filter["stopEvents"][0]["transportation"]["number"] = true;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Authorization", "apikey " SECRET_TFNSW_API_KEY);
  http.setTimeout(15000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DBG_WARN("fetchStop[%d] HTTP %d", idx, code);
    http.end();
    return false;
  }

  int contentLen = http.getSize();
  bool chunked   = (contentLen < 0);
  DBG_INFO("fetchStop[%d] %s, heap: %u",
           idx, chunked ? "chunked" : String(String(contentLen) + " bytes").c_str(),
           ESP.getFreeHeap());

  // Parse JSON directly from the network stream — only the filter doc (4KB)
  // and the TLS buffers are in memory.  No large response buffer needed.
  DynamicJsonDocument doc(4096);
  DeserializationError err;

  if (chunked) {
    DeChunkStream dcs(http.getStream());
    err = deserializeJson(doc, dcs, DeserializationOption::Filter(filter));
  } else {
    err = deserializeJson(doc, http.getStream(),
                          DeserializationOption::Filter(filter));
  }
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

  memset(&stopData[idx], 0, sizeof(StopData));

  for (JsonObject ev : events) {
    if (count >= DEPARTURES_PER_STOP) break;

    const char* dtStr = ev["departureTimeEstimated"] | ev["departureTimePlanned"];
    if (!dtStr) continue;

    time_t depEpoch = parseISODatetime(dtStr);
    if (depEpoch == 0) continue;

    int mins = (int)((depEpoch - nowUTC) / 60);
    if (mins < 0) continue;

    Departure& dep   = stopData[idx].departures[count];
    dep.epochUTC     = depEpoch;
    dep.minutesUntil = mins;

    formatLocalHHMM(depEpoch, dep.clockTime, sizeof(dep.clockTime));

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

  DBG_INFO("Stop %s — %d departure(s):", STOP_NAMES[idx], count);
  for (uint8_t i = 0; i < count; i++) {
    const Departure& d = stopData[idx].departures[i];
    DBG_INFO("  [%d] Route %-6s  %dm  %s", i + 1, d.route, d.minutesUntil, d.clockTime);
  }
  return stopData[idx].valid;
}

void fetchAllStops() {
  DBG_INFO("Fetching all %d stops...", STOP_COUNT);
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    fetchStop(i);
    if (i < STOP_COUNT - 1) {
      delay(INTER_REQUEST_MS);
    }
  }
}

void recalcMinutes() {
  time_t nowUTC = getUTCNow();
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    for (uint8_t j = 0; j < stopData[i].count; j++) {
      Departure& d = stopData[i].departures[j];
      if (d.valid) {
        d.minutesUntil = (int)((d.epochUTC - nowUTC) / 60);
      }
    }
  }
}
