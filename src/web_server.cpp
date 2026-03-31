#include "web_server.h"
#include "bus_api.h"
#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static AsyncWebServer server(WEB_PORT);

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

// GET /api/state
// Returns current time, date, and all stop departure data as JSON.
// Used by the Phase 3 canvas mirror and any external consumers.
static void handleApiState(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(2048);

  doc["time"] = getTimeStr();
  doc["date"] = getDateStr();

  JsonArray stops = doc.createNestedArray("stops");
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = stops.createNestedObject();
    stop["id"]    = STOP_IDS[i];
    stop["name"]  = STOP_NAMES[i];
    stop["valid"] = stopData[i].valid;

    JsonArray deps = stop.createNestedArray("departures");
    for (uint8_t j = 0; j < stopData[i].count; j++) {
      const Departure& d = stopData[i].departures[j];
      JsonObject dep = deps.createNestedObject();
      dep["route"]   = d.route;
      dep["clock"]   = d.clockTime;
      dep["minutes"] = d.minutesUntil;
    }
  }

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

// GET /
// Phase 2 placeholder — will become the full config page.
static void handleRoot(AsyncWebServerRequest* req) {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CYD BusStop</title>"
    "<style>body{font-family:sans-serif;max-width:480px;margin:2em auto;padding:0 1em}"
    "a{display:block;margin:.5em 0;padding:.6em 1em;background:#222;color:#fff;"
    "text-decoration:none;border-radius:4px}</style>"
    "</head><body>"
    "<h2>CYD BusStop</h2>"
    "<a href='/api/state'>Current state (JSON)</a>"
    "<a href='/mirror'>Display mirror <em>(Phase 3)</em></a>"
    "<a href='/update'>OTA firmware update</a>"
    "<p style='color:#999;font-size:.85em'>Config page coming in Phase 2.</p>"
    "</body></html>";

  req->send(200, "text/html", html);
}

// GET /mirror
// Phase 3 placeholder — will render a JS canvas that mirrors the TFT layout.
static void handleMirror(AsyncWebServerRequest* req) {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<title>Display Mirror</title>"
    "</head><body style='background:#000;color:#fff;font-family:monospace'>"
    "<h3>Display mirror — Phase 3</h3>"
    "<p>Will poll <a href='/api/state' style='color:#0cf'>/api/state</a> "
    "and render a canvas replicating the TFT layout.</p>"
    "</body></html>";

  req->send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void initWebServer() {
  server.on("/",          HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleApiState);
  server.on("/mirror",    HTTP_GET, handleMirror);

  // ElegantOTA attaches /update and /update/identity routes
  ElegantOTA.begin(&server);

  server.begin();
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void handleWebServer() {
  ElegantOTA.loop();  // required for OTA progress callbacks
}
