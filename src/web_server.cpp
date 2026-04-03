#include "web_server.h"
#include "bus_api.h"
#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static AsyncWebServer server(WEB_PORT);
static volatile bool s_stopRefreshRequested = false;

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

// GET /api/state
// Returns current time, date, and all stop departure data as JSON.
// Used by the Phase 3 canvas mirror and any external consumers.
static void handleApiState(AsyncWebServerRequest* req) {
  // Recalculate minutes so the JSON always reflects current time
  recalcMinutes();

  DynamicJsonDocument doc(3072);

  doc["time"]  = getTimeStr();
  doc["date"]  = getDateStr();
  doc["now"]   = (long)getUTCNow();   // UTC epoch for client-side recalc
  doc["tzOff"] = getLocalTZOffset();  // local TZ offset in seconds (e.g. +39600 for AEDT)

  JsonArray stops = doc.createNestedArray("stops");
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = stops.createNestedObject();
    stop["id"]    = stopIds[i];
    stop["name"]  = stopNames[i];
    stop["valid"] = stopData[i].valid;
    stop["fetchAge"] = stopData[i].lastFetchMs
      ? (millis() - stopData[i].lastFetchMs) / 1000 : -1;
    if (stopData[i].hasAlerts) {
      stop["alert"] = stopData[i].alertText;
    }

    JsonArray deps = stop.createNestedArray("departures");
    for (uint8_t j = 0; j < stopData[i].count; j++) {
      const Departure& d = stopData[i].departures[j];
      JsonObject dep = deps.createNestedObject();
      dep["route"]   = d.route;
      dep["clock"]   = d.clockTime;
      dep["minutes"] = d.minutesUntil;
      dep["epoch"]   = (long)d.epochUTC;
      dep["rt"]      = d.isRealtime;
      dep["delay"]   = d.delaySecs;
      if (d.destination[0] != '\0') {
        dep["dest"] = d.destination;
      }
    }
  }

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

// GET /api/stops
static void handleApiStops(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(1024);
  JsonArray stops = doc.to<JsonArray>();

  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = stops.createNestedObject();
    stop["id"]   = stopIds[i];
    stop["name"] = stopNames[i];
  }

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

static void handleApiStopsUpdate(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    DBG_WARN("/api/stops POST JSON parse failed: %s", err.c_str());
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (!doc.is<JsonArray>()) {
    req->send(400, "application/json", "{\"error\":\"Expected JSON array\"}");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() > STOP_COUNT) {
    req->send(400, "application/json", "{\"error\":\"Too many stops\"}");
    return;
  }

  bool anyChanged = false;
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    if (i < arr.size()) {
      JsonObject obj = arr[i].as<JsonObject>();
      const char* id = obj["id"];
      const char* name = obj["name"];
      if (!id || !name || strlen(id) == 0 || strlen(name) == 0 || strlen(id) >= STOP_ID_MAX || strlen(name) >= STOP_NAME_MAX) {
        req->send(400, "application/json", "{\"error\":\"Invalid stop id/name\"}");
        return;
      }
      bool idChanged = (strcmp(stopIds[i], id) != 0);
      bool nameChanged = (strcmp(stopNames[i], name) != 0);
      if (idChanged || nameChanged) {
        DBG_INFO("Stop config change[%d]: id '%s' -> '%s', name '%s' -> '%s'",
                 i, stopIds[i], id, stopNames[i], name);
        anyChanged = true;
      }
      setStopConfig(i, id, name);
    } else {
      // If fewer entries, keep existing or defaults. Keep previous values.
    }
  }

  if (!anyChanged) {
    DBG_INFO("Stop config: POST received but no stop values changed");
  }

  if (!saveStopConfig()) {
    DBG_WARN("/api/stops POST: saveStopConfig failed");
  }

  s_stopRefreshRequested = true;
  req->send(200, "application/json", "{\"result\":\"ok\",\"refresh\":\"queued\"}");
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
.alert{background:#442200;border:1px solid #886600;border-radius:4px;padding:.4em .6em;margin-bottom:.5em;font-size:.8em;color:#ffcc44}
.stop{background:#1a1a1a;border-radius:6px;padding:.6em .8em;margin-bottom:.5em}
.sh{display:flex;justify-content:space-between;align-items:center;margin-bottom:.3em}
.sn{color:#0cf;font-weight:700}
.badge{font-size:.7em;padding:.1em .4em;border-radius:3px}
.badge-rt{background:#0a3a0a;color:#0f0;border:1px solid #0a0}
.badge-sched{background:#222;color:#666;border:1px solid #444}
.dep{display:flex;gap:.6em;padding:.15em 0;font-size:.9em;align-items:baseline}
.dr{min-width:2.8em;font-weight:600}
.ds{min-width:4.2em}
.dd{flex:1;color:#888;font-size:.8em;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.dy{min-width:5.2em;text-align:right}
.mn{min-width:2.8em;text-align:right}
.delay{display:inline-block;font-size:.72em;padding:.1em .4em;border-radius:999px;border:1px solid #6b4a00;background:#2d2100;color:#ffb84d}
.delay-early{display:inline-block;font-size:.72em;padding:.1em .4em;border-radius:999px;border:1px solid #1b5e20;background:#0f2412;color:#7dff8a}
.near{color:#0f0}.far{color:#ff0}.now{color:#f90}.gone{color:#f44}
.ck{color:#aaa}
.nd{color:#555;font-style:italic;font-size:.85em}
.ft{margin-top:.8em;font-size:.75em;color:#444;text-align:center}
.ft a{color:#0cf;text-decoration:none}
</style></head><body>
<h1>Bus Departures</h1>
<div class="hdr"><span class="ts" id="clock">--</span><span class="upd" id="upd">--</span></div>
<div id="alerts"></div>
<div id="stops">Loading...</div>
<div class="ft"><a href="/api/state">JSON</a></div>
<div class="ft"><button id="btnToggleEdit" onclick="toggleEditPane()">Edit stops</button></div>
<div id="stopConfig" style="display:none;margin-top:.8em;background:#222;border:1px solid #444;border-radius:6px;padding:.8em;">
  <h2 style="font-size:1em;margin-bottom:.4em;">Edit stops</h2>
  <div id="stopConfigRows"></div>
  <div style="margin-top:.5em;display:flex;gap:.5em;align-items:center;">
    <button onclick="saveStopConfig()">Save</button>
    <button onclick="resetStopConfig()">Reset defaults</button>
    <span id="configStatus" style="color:#8f8;font-size:.85em;"></span>
  </div>
</div>
<script>
var D=null,fetched=0,stopsConfig=[];
function toggleEditPane(){
  var pane=document.getElementById('stopConfig');
  pane.style.display = (pane.style.display==='none'?'block':'none');
}
function renderStopConfigRows(){
  var rows=document.getElementById('stopConfigRows');
  if (!Array.isArray(stopsConfig) || stopsConfig.length===0) {
    rows.innerHTML='<div class="nd">No stop data</div>';
    return;
  }
  var h='';
  stopsConfig.forEach(function(s,i){
    h += '<div style="margin-bottom:.4em;display:flex;gap:.4em;align-items:center;">';
    h += '<input id="stopId'+i+'" style="flex:1;" value="'+(s.id||'')+'" placeholder="Stop ID">';
    h += '<input id="stopName'+i+'" style="flex:2;" value="'+(s.name||'')+'" placeholder="Stop name">';
    h += '</div>';
  });
  rows.innerHTML = h;
}
function loadStopConfig(){
  fetch('/api/stops').then(function(r){return r.json();}).then(function(arr){
    stopsConfig = arr;
    renderStopConfigRows();
  }).catch(function(err){
    document.getElementById('configStatus').textContent='Load failed';
    console.warn(err);
  });
}
function saveStopConfig(){
  var payload=[];
  for(var i=0;i<stopsConfig.length;i++){
    var idInput=document.getElementById('stopId'+i);
    var nameInput=document.getElementById('stopName'+i);
    if (!idInput || !nameInput) continue;
    payload.push({id:idInput.value.trim(),name:nameInput.value.trim()});
  }
  fetch('/api/stops',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('configStatus').textContent='Saved';
    loadStopConfig();
    fetchAndRender();
  }).catch(function(e){
    document.getElementById('configStatus').textContent='Save failed';
    console.warn(e);
  });
}
function resetStopConfig(){
  fetch('/api/stops/reset',{method:'POST'})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('configStatus').textContent='Reset';
    loadStopConfig();
    fetchAndRender();
  }).catch(function(e){
    document.getElementById('configStatus').textContent='Reset failed';
    console.warn(e);
  });
}
function fetchAndRender(){
  fetch('/api/state').then(function(r){return r.json();}).then(function(d){
    D=d;fetched=Date.now();render();
  }).catch(function(){
  });
}
function fmtDelay(s){
  if(!s||s===0)return '';
  var m=Math.round(s/60);
  if(m>=2)return '<span class="delay">+'+m+'m late</span>';
  if(m<=-2)return '<span class="delay-early">'+Math.abs(m)+'m early</span>';
  return '';
}
function isToday(epoch,nowUTC,tzOff){
  var localEp=(epoch+tzOff)*1000;
  var localNow=(nowUTC+tzOff)*1000;
  var d1=new Date(localEp),d2=new Date(localNow);
  return d1.getUTCFullYear()===d2.getUTCFullYear()
      && d1.getUTCMonth()===d2.getUTCMonth()
      && d1.getUTCDate()===d2.getUTCDate();
}
function dayAbbr(epoch,tzOff){
  var days=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  var d=new Date((epoch+tzOff)*1000);
  return days[d.getUTCDay()];
}
function render(){
  if(!D)return;
  document.getElementById('clock').textContent=D.time+'  '+D.date;
  var age=Math.round((Date.now()-fetched)/1000);
  document.getElementById('upd').textContent='Updated '+age+'s ago';
  var nowUTC=D.now+age;
  var tzOff=D.tzOff||0;
  var ah='';
  D.stops.forEach(function(s){
    if(s.alert)ah+='<div class="alert">&#9888; '+s.alert+'</div>';
  });
  document.getElementById('alerts').innerHTML=ah;
  var h='';
  D.stops.forEach(function(s){
    h+='<div class="stop"><div class="sh"><span class="sn">'+s.name+'</span></div>';
    if(!s.valid||!s.departures.length){h+='<div class="nd">No data</div>';}
    else s.departures.forEach(function(d){
      var m=Math.round((d.epoch-nowUTC)/60);
      var today=isToday(d.epoch,nowUTC,tzOff);
      var cls,label;
      if(!today){
        cls='far';label=dayAbbr(d.epoch,tzOff);
      } else if(m<=0){
        if(m<0){cls='gone';label='Gone';}else{cls='now';label='Now';}
      } else {
        cls=m<10?'near':'far';label=m+'m';
      }
      var dl=fmtDelay(d.delay);
      var dest=d.dest||'';
      var badge=d.rt?'<span class="badge badge-rt">LIVE</span>'
                    :'<span class="badge badge-sched">SCHED</span>';
      h+='<div class="dep">'
        +'<span class="dr">'+d.route+'</span>'
        +'<span class="ds">'+badge+'</span>'
        +'<span class="dd">'+dest+'</span>'
        +'<span class="dy">'+dl+'</span>'
        +'<span class="mn '+cls+'">'+label+'</span>'
        +'<span class="ck">'+d.clock+'</span>'
        +'</div>';
    });
    h+='</div>';
  });
  document.getElementById('stops').innerHTML=h;
}
function poll(){
  fetchAndRender();
}

loadStopConfig();
poll();
setInterval(poll,15000);
setInterval(render,5000);
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
  server.on("/api/stops", HTTP_GET, handleApiStops);
  server.on("/api/stops", HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr, handleApiStopsUpdate);
  server.on("/api/stops/reset", HTTP_POST, [](AsyncWebServerRequest* req){
    if (!resetStopConfig() || !saveStopConfig()) {
      req->send(500, "application/json", "{\"error\":\"reset failed\"}");
      return;
    }
    s_stopRefreshRequested = true;
    req->send(200, "application/json", "{\"result\":\"reset\",\"refresh\":\"queued\"}");
  });
  server.on("/mirror",    HTTP_GET, handleMirror);

  server.begin();
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void handleWebServer() {
  // AsyncWebServer handles requests via interrupt — nothing needed here.
  // ArduinoOTA is handled in loop() via main.cpp.
}

bool consumeStopRefreshRequest() {
  bool requested = s_stopRefreshRequested;
  s_stopRefreshRequested = false;
  return requested;
}
