#include "web_server.h"
#include "bus_api.h"
#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

#include <ESPAsyncWebServer.h>
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
  // Recalculate minutes so the JSON always reflects current time
  recalcMinutes();

  DynamicJsonDocument doc(2048);

  doc["time"] = getTimeStr();
  doc["date"] = getDateStr();
  doc["now"]  = (long)getUTCNow();  // UTC epoch for client-side recalc

  JsonArray stops = doc.createNestedArray("stops");
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = stops.createNestedObject();
    stop["id"]    = STOP_IDS[i];
    stop["name"]  = STOP_NAMES[i];
    stop["valid"] = stopData[i].valid;
    stop["fetchAge"] = stopData[i].lastFetchMs
      ? (millis() - stopData[i].lastFetchMs) / 1000 : -1;

    JsonArray deps = stop.createNestedArray("departures");
    for (uint8_t j = 0; j < stopData[i].count; j++) {
      const Departure& d = stopData[i].departures[j];
      JsonObject dep = deps.createNestedObject();
      dep["route"]   = d.route;
      dep["clock"]   = d.clockTime;
      dep["minutes"] = d.minutesUntil;
      dep["epoch"]   = (long)d.epochUTC;
    }
  }

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

// GET /
// Dashboard — polls /api/state every 60s, recalculates minutes every 15s client-side.
static const char ROOT_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD BusStop</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#111;color:#fff;padding:1em;max-width:480px;margin:0 auto}
h1{font-size:1.2em;margin-bottom:.15em}
.hdr{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:.6em}
.ts{color:#888;font-size:.85em}
.upd{color:#555;font-size:.75em}
.stop{background:#1a1a1a;border-radius:6px;padding:.6em .8em;margin-bottom:.5em}
.sn{color:#0cf;font-weight:700;margin-bottom:.3em}
.dep{display:flex;gap:.8em;padding:.1em 0;font-size:.9em}
.rt{min-width:3em;font-weight:600}
.mn{min-width:3em;text-align:right}
.near{color:#0f0}.far{color:#ff0}.gone{color:#f44}
.ck{color:#aaa}
.nd{color:#555;font-style:italic;font-size:.85em}
.ft{margin-top:.8em;font-size:.75em;color:#444;text-align:center}
.ft a{color:#0cf;text-decoration:none}
</style></head><body>
<h1>Bus Departures</h1>
<div class="hdr"><span class="ts" id="clock">--</span><span class="upd" id="upd">--</span></div>
<div id="stops">Loading...</div>
<div class="ft"><a href="/api/state">JSON</a></div>
<script>
var D=null,fetched=0;
function render(){
  if(!D)return;
  document.getElementById('clock').textContent=D.time+'  '+D.date;
  var age=Math.round((Date.now()-fetched)/1000);
  document.getElementById('upd').textContent='Updated '+age+'s ago';
  var nowUTC=D.now+age;
  var h='';
  D.stops.forEach(function(s){
    h+='<div class="stop"><div class="sn">'+s.name+'</div>';
    if(!s.valid||!s.departures.length){h+='<div class="nd">No data</div>';}
    else s.departures.forEach(function(d){
      var m=Math.round((d.epoch-nowUTC)/60);
      var cls=m<=0?'gone':m<10?'near':'far';
      var label=m<0?'Gone':m==0?'Now':m+'m';
      h+='<div class="dep"><span class="rt">'+d.route+'</span>'
        +'<span class="mn '+cls+'">'+label+'</span>'
        +'<span class="ck">'+d.clock+'</span></div>';
    });
    h+='</div>';
  });
  document.getElementById('stops').innerHTML=h;
}
function poll(){
  fetch('/api/state').then(function(r){return r.json();}).then(function(d){
    D=d;fetched=Date.now();render();
  }).catch(function(){});
}
poll();
setInterval(poll,60000);
setInterval(render,15000);
</script>
</body></html>)rawliteral";

static void handleRoot(AsyncWebServerRequest* req) {
  req->send(200, "text/html", ROOT_HTML);
}

// GET /mirror — redirect to root (now serves live data)
static void handleMirror(AsyncWebServerRequest* req) {
  req->redirect("/");
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void initWebServer() {
  server.on("/",          HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleApiState);
  server.on("/mirror",    HTTP_GET, handleMirror);

  server.begin();
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void handleWebServer() {
  // AsyncWebServer handles requests via interrupt — nothing needed here.
  // ArduinoOTA is handled in loop() via main.cpp.
}
