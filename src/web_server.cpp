#include "web_server.h"
#include "bus_api.h"
#include "display.h"
#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static AsyncWebServer server(WEB_PORT);
static volatile bool s_stopRefreshRequested    = false;
static volatile bool s_displayRefreshRequested = false;

static void handleApiState(AsyncWebServerRequest* req) {
  recalcMinutes();

  DynamicJsonDocument doc(8192);

  doc["time"]  = getTimeStr();
  doc["date"]  = getDateStr();
  doc["now"]   = (long)getUTCNow();
  doc["tzOff"] = getLocalTZOffset();
  doc["webuiDepartureCount"] = webuiDepartureCount;

  JsonArray stops = doc.createNestedArray("stops");
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = stops.createNestedObject();
    stop["id"]       = stopIds[i];
    stop["name"]     = stopNames[i];
    stop["valid"]    = stopData[i].valid;
    stop["fetchAge"] = stopData[i].lastFetchMs
      ? (millis() - stopData[i].lastFetchMs) / 1000 : -1;
    if (stopData[i].hasAlerts) {
      stop["alert"] = stopData[i].alertText;
    }

    JsonArray deps = stop.createNestedArray("departures");
    uint8_t webCount = (stopData[i].count < webuiDepartureCount) ? stopData[i].count : webuiDepartureCount;
    for (uint8_t j = 0; j < webCount; j++) {
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

static void handleApiSettings(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(256);
  doc["brightness"]         = displayBrightness;
  doc["brightnessDefault"]  = BRIGHTNESS_DEFAULT;
  doc["time24Hour"]         = time24Hour;
  doc["time24HourDefault"]  = TIME_24HR_DEFAULT;
  doc["webuiDepartureCount"]        = webuiDepartureCount;
  doc["webuiDepartureCountDefault"] = WEBUI_DEPARTURES_DEFAULT;
  doc["webuiDepartureCountMax"]     = MAX_STORED_DEPARTURES;

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

static void handleApiStats(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(1024);
  uint32_t nowMs = millis();

  int32_t newestFetchAge = -1;
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    if (!stopData[i].lastFetchMs) continue;
    int32_t age = (int32_t)((nowMs - stopData[i].lastFetchMs) / 1000);
    if (newestFetchAge < 0 || age < newestFetchAge) {
      newestFetchAge = age;
    }
  }

  doc["ip"]              = WiFi.localIP().toString();
  doc["ssid"]            = WiFi.SSID();
  doc["rssi"]            = WiFi.RSSI();
  doc["uptimeSec"]       = nowMs / 1000;
  doc["freeHeap"]        = ESP.getFreeHeap();
  doc["maxAllocHeap"]    = ESP.getMaxAllocHeap();
  doc["pollIntervalSec"] = POLL_INTERVAL_MS / 1000;
  doc["stopCount"]       = STOP_COUNT;
  doc["lastFetchAgeSec"] = newestFetchAge;
  doc["webuiDepartureCount"] = webuiDepartureCount;
  doc["maxStoredDepartures"] = MAX_STORED_DEPARTURES;
  doc["tftDepartures"]       = TFT_DEPARTURES_PER_STOP;
  doc["time"]            = getTimeStr();
  doc["date"]            = getDateStr();

  JsonArray perStop = doc.createNestedArray("stops");
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    JsonObject stop = perStop.createNestedObject();
    stop["name"]     = stopNames[i];
    stop["valid"]    = stopData[i].valid;
    stop["fetchAge"] = stopData[i].lastFetchMs
      ? (nowMs - stopData[i].lastFetchMs) / 1000 : -1;
    stop["alerts"]   = stopData[i].hasAlerts;
    stop["deps"]     = stopData[i].count;
  }

  String body;
  serializeJson(doc, body);
  req->send(200, "application/json", body);
}

static void handleApiStopsUpdate(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  (void)index;
  (void)total;

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
    if (i >= arr.size()) continue;

    JsonObject obj   = arr[i].as<JsonObject>();
    const char* id   = obj["id"];
    const char* name = obj["name"];
    if (!id || !name || strlen(id) == 0 || strlen(name) == 0 ||
        strlen(id) >= STOP_ID_MAX || strlen(name) >= STOP_NAME_MAX) {
      req->send(400, "application/json", "{\"error\":\"Invalid stop id/name\"}");
      return;
    }

    bool idChanged   = (strcmp(stopIds[i], id) != 0);
    bool nameChanged = (strcmp(stopNames[i], name) != 0);
    if (idChanged || nameChanged) {
      DBG_INFO("Stop config change[%d]: id '%s' -> '%s', name '%s' -> '%s'",
               i, stopIds[i], id, stopNames[i], name);
      anyChanged = true;
    }
    setStopConfig(i, id, name);
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

static void handleApiSettingsUpdate(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  (void)index;
  (void)total;

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    DBG_WARN("/api/settings POST JSON parse failed: %s", err.c_str());
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (!doc.is<JsonObject>()) {
    req->send(400, "application/json", "{\"error\":\"Expected JSON object\"}");
    return;
  }

  JsonObject obj = doc.as<JsonObject>();
  if (!obj.containsKey("brightness") || !obj.containsKey("time24Hour") || !obj.containsKey("webuiDepartureCount")) {
    req->send(400, "application/json", "{\"error\":\"Missing settings fields\"}");
    return;
  }

  int brightness = obj["brightness"];
  if (brightness < 0 || brightness > 255) {
    req->send(400, "application/json", "{\"error\":\"Brightness must be 0-255\"}");
    return;
  }

  int webuiCount = obj["webuiDepartureCount"];
  if (webuiCount < 1 || webuiCount > MAX_STORED_DEPARTURES) {
    req->send(400, "application/json", "{\"error\":\"WebUI departures out of range\"}");
    return;
  }

  bool newTime24Hour = obj["time24Hour"];
  bool webuiCountChanged = (webuiDepartureCount != (uint8_t)webuiCount);
  bool brightnessChanged = (displayBrightness != (uint8_t)brightness);
  bool timeFormatChanged = (time24Hour != newTime24Hour);

  setDisplayBrightnessSetting((uint8_t)brightness);
  setTime24HourSetting(newTime24Hour);
  setWebuiDepartureCountSetting((uint8_t)webuiCount);

  if (!saveUserSettings()) {
    DBG_WARN("/api/settings POST: saveUserSettings failed");
  }

  if (brightnessChanged) {
    setBrightness(displayBrightness);
  }

  if (brightnessChanged || timeFormatChanged || webuiCountChanged) {
    s_displayRefreshRequested = true;
  }

  req->send(200, "application/json", "{\"result\":\"ok\"}");
}

static const char ROOT_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD BusStop</title>
<style>
:root{--bg:#101418;--panel:#182028;--line:#283240;--text:#f3f6f8;--muted:#8ea0af;--accent:#59d0ff;--good:#71f58c;--warn:#ffd35c;--now:#ff9b47;--gone:#ff6666}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:linear-gradient(180deg,#0b1015 0%,#101418 55%,#17212b 100%);color:var(--text);padding:16px;max-width:560px;margin:0 auto}
h1{font-size:1.35rem;margin-bottom:.2rem}
a{color:var(--accent);text-decoration:none}
.hdr{display:flex;justify-content:space-between;align-items:flex-end;gap:12px;margin-bottom:14px}
.ts{color:var(--muted);font-size:.92rem}
.upd{color:#667687;font-size:.78rem}
.topnav{display:flex;justify-content:flex-end;margin-bottom:12px}
.topnav a{padding:.45rem .7rem;border:1px solid var(--line);border-radius:999px;background:rgba(255,255,255,.03)}
.alert{background:#4b2d07;border:1px solid #8c6518;border-radius:10px;padding:.55rem .7rem;margin-bottom:.55rem;font-size:.84rem;color:#ffd35c}
.stop{background:rgba(24,32,40,.92);border:1px solid var(--line);border-radius:14px;padding:.8rem .9rem;margin-bottom:.65rem;box-shadow:0 12px 28px rgba(0,0,0,.18)}
.sh{display:flex;justify-content:space-between;align-items:center;margin-bottom:.45rem}
.sn{color:var(--accent);font-weight:700}
.dep{display:grid;grid-template-columns:34px 46px minmax(0,1fr) max-content 42px 42px;column-gap:.28rem;padding:.2rem 0;font-size:.82rem;align-items:center}
.dr{font-weight:700}
.ds{min-width:46px}
.dd{min-width:0;color:var(--muted);font-size:.76rem;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.dy{min-width:0;text-align:right;white-space:nowrap}
.mn{min-width:42px;text-align:right;white-space:nowrap}
.badge{font-size:.62rem;padding:.1rem .3rem;border-radius:999px;display:inline-block;white-space:nowrap}
.badge-rt{background:#0f3218;color:#71f58c;border:1px solid #245c31}
.badge-sched{background:#212a34;color:#93a2b1;border:1px solid #394553}
.delay{display:inline-block;font-size:.66rem;padding:.08rem .34rem;border-radius:999px;border:1px solid #6b4a00;background:#342708;color:#ffbe56;white-space:nowrap}
.delay-early{display:inline-block;font-size:.66rem;padding:.08rem .34rem;border-radius:999px;border:1px solid #23572c;background:#122617;color:#7dff8a;white-space:nowrap}
.near{color:var(--good)}.far{color:var(--warn)}.now{color:var(--now)}.gone{color:var(--gone)}
.ck{color:#b7c3ce;white-space:nowrap;text-align:right}
.nd{color:#61717f;font-style:italic;font-size:.85rem}
</style></head><body>
<div class="topnav"><a href="/config">Config</a></div>
<h1>Bus Departures</h1>
<div class="hdr"><span class="ts" id="clock">--</span><span class="upd" id="upd">--</span></div>
<div id="alerts"></div>
<div id="stops">Loading...</div>
<script>
var D=null,fetched=0;
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
function fmtMinutes(m){
  if(m>60){
    var h=Math.floor(m/60);
    var mins=m%60;
    return h+'h'+String(mins).padStart(2,'0')+'m';
  }
  return m+'m';
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
        cls=m<10?'near':'far';label=fmtMinutes(m);
      }
      var dl=fmtDelay(d.delay);
      var dest=d.dest||'';
      var badge=d.rt?'<span class="badge badge-rt">LIVE</span>':'<span class="badge badge-sched">SCHED</span>';
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
  fetch('/api/state').then(function(r){return r.json();}).then(function(d){
    D=d;fetched=Date.now();render();
  }).catch(function(){});
}
poll();
setInterval(poll,15000);
setInterval(render,5000);
</script>
</body></html>)rawliteral";

static const char CONFIG_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD BusStop Config</title>
<style>
:root{--bg:#f2efe8;--panel:#fffdf8;--line:#d7d0c2;--text:#18212a;--muted:#6d747d;--accent:#0b7e87;--accent-2:#d66c31;--ok:#1f8f53;--warn:#8b5e13;--shadow:0 14px 32px rgba(40,28,18,.08)}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Georgia,"Times New Roman",serif;background:radial-gradient(circle at top right,#f6d9bf 0%,#f2efe8 42%,#e5ecec 100%);color:var(--text);padding:18px;max-width:720px;margin:0 auto}
h1,h2{font-family:"Avenir Next","Trebuchet MS",sans-serif;letter-spacing:.02em}
h1{font-size:1.55rem}
h2{font-size:1rem;margin-bottom:.8rem}
a{color:var(--accent);text-decoration:none}
button,input{font:inherit}
.topbar{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;margin-bottom:16px}
.back{padding:.5rem .75rem;border:1px solid var(--line);border-radius:999px;background:rgba(255,255,255,.62)}
.sub{color:var(--muted);margin-top:.25rem;font-size:.92rem}
.grid{display:grid;gap:14px}
.card{background:rgba(255,253,248,.9);border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:var(--shadow);backdrop-filter:blur(6px)}
.stats{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.stat{padding:12px;border:1px solid var(--line);border-radius:14px;background:#fffaf1}
.stat .label{display:block;color:var(--muted);font-size:.76rem;text-transform:uppercase;letter-spacing:.08em;margin-bottom:.3rem}
.stat .value{display:block;font-family:"Avenir Next","Trebuchet MS",sans-serif;font-size:1rem;font-weight:700}
.stopstat{display:grid;grid-template-columns:1fr auto auto;gap:8px;padding:.55rem 0;border-top:1px solid #ece5d7}
.stopstat:first-of-type{border-top:none;padding-top:0}
.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
.field{margin-bottom:12px}
.field label{display:block;font-family:"Avenir Next","Trebuchet MS",sans-serif;font-size:.82rem;color:var(--muted);margin-bottom:.35rem}
.field input[type="text"]{width:100%;padding:.65rem .7rem;border:1px solid #cfc5b3;border-radius:12px;background:#fff}
.stoprow{display:grid;grid-template-columns:130px 1fr;gap:10px;margin-bottom:10px}
.rangeWrap{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:center}
.rangeWrap input[type="range"]{width:100%}
.pill{display:inline-block;padding:.18rem .5rem;border-radius:999px;font-size:.76rem;border:1px solid #c9c0b0;background:#fff}
.toggle{display:flex;align-items:center;gap:10px}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:8px}
.btn{padding:.62rem .88rem;border-radius:12px;border:1px solid #cdbca1;background:#fff8ee;color:var(--text)}
.btn.primary{background:linear-gradient(135deg,var(--accent),#0e9aa2);border-color:#0c6b72;color:#fff}
.btn.warn{background:linear-gradient(135deg,#f0d7ac,var(--accent-2));border-color:#ba6330;color:#fff}
.status{min-height:1.25rem;color:var(--ok);font-size:.86rem}
pre{white-space:pre-wrap;word-break:break-word;max-height:360px;overflow:auto;background:#1f2630;color:#dfe7ef;padding:12px;border-radius:14px;font-size:.8rem}
@media (max-width:560px){
  .stats{grid-template-columns:1fr}
  .stoprow{grid-template-columns:1fr}
}
</style></head><body>
<div class="topbar">
  <div>
    <h1>Config & Stats</h1>
    <div class="sub">Adjust display settings, edit stops, and inspect live device state.</div>
  </div>
  <a class="back" href="/">Back to departures</a>
</div>
<div class="grid">
  <section class="card">
    <h2>System Stats</h2>
    <div class="stats">
      <div class="stat"><span class="label">Local Time</span><span class="value" id="statTime">--</span></div>
      <div class="stat"><span class="label">IP Address</span><span class="value" id="statIp">--</span></div>
      <div class="stat"><span class="label">WiFi</span><span class="value" id="statWifi">--</span></div>
      <div class="stat"><span class="label">Uptime</span><span class="value" id="statUptime">--</span></div>
      <div class="stat"><span class="label">Free Heap</span><span class="value" id="statHeap">--</span></div>
      <div class="stat"><span class="label">Last Fetch</span><span class="value" id="statFetch">--</span></div>
      <div class="stat"><span class="label">WebUI Rows</span><span class="value" id="statRows">--</span></div>
      <div class="stat"><span class="label">TFT Rows</span><span class="value" id="statTftRows">--</span></div>
    </div>
    <div id="stopStats" style="margin-top:12px"></div>
  </section>

  <section class="card">
    <h2>Display Settings</h2>
    <div class="field">
      <label for="brightness">Brightness</label>
      <div class="rangeWrap">
        <input id="brightness" type="range" min="0" max="255" step="1" oninput="document.getElementById('brightnessValue').textContent=this.value">
        <span class="pill" id="brightnessValue">--</span>
      </div>
    </div>
    <div class="field toggle">
      <input id="time24Hour" type="checkbox">
      <label for="time24Hour" style="margin:0">Use 24-hour clock on TFT and WebUI</label>
    </div>
    <div class="field">
      <label for="webuiDepartureCount">Departures shown on WebUI</label>
      <div class="rangeWrap">
        <input id="webuiDepartureCount" type="range" min="1" max="8" step="1" oninput="document.getElementById('webuiDepartureCountValue').textContent=this.value">
        <span class="pill" id="webuiDepartureCountValue">--</span>
      </div>
    </div>
    <div class="actions">
      <button class="btn primary" onclick="saveSettings()">Save settings</button>
      <button class="btn" onclick="resetSettings()">Reset defaults</button>
    </div>
    <div class="status" id="settingsStatus"></div>
  </section>

  <section class="card">
    <h2>Edit Stops</h2>
    <div id="stopConfigRows"></div>
    <div class="actions">
      <button class="btn primary" onclick="saveStopConfig()">Save stops</button>
      <button class="btn warn" onclick="resetStopConfig()">Reset default stops</button>
    </div>
    <div class="status" id="stopStatus"></div>
  </section>

  <section class="card">
    <div class="row" style="justify-content:space-between;margin-bottom:10px">
      <h2 style="margin:0">Raw JSON</h2>
      <button class="btn" onclick="loadJson()">Refresh JSON</button>
    </div>
    <pre id="jsonState">Loading...</pre>
  </section>
</div>
<script>
var stopsConfig=[];
function esc(v){
  return String(v||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function fmtDuration(sec){
  sec=Math.max(0,sec||0);
  var d=Math.floor(sec/86400);
  var h=Math.floor((sec%86400)/3600);
  var m=Math.floor((sec%3600)/60);
  if(d>0)return d+'d '+h+'h';
  if(h>0)return h+'h '+m+'m';
  return m+'m';
}
function fmtFetchAge(sec){
  if(sec===undefined||sec===null||sec<0)return 'No successful fetch';
  if(sec<60)return sec+'s ago';
  if(sec<3600)return Math.round(sec/60)+'m ago';
  return Math.round(sec/3600)+'h ago';
}
function loadSettings(){
  fetch('/api/settings').then(function(r){return r.json();}).then(function(d){
    document.getElementById('brightness').value=d.brightness;
    document.getElementById('brightnessValue').textContent=d.brightness;
    document.getElementById('time24Hour').checked=!!d.time24Hour;
    document.getElementById('webuiDepartureCount').max=d.webuiDepartureCountMax;
    document.getElementById('webuiDepartureCount').value=d.webuiDepartureCount;
    document.getElementById('webuiDepartureCountValue').textContent=d.webuiDepartureCount;
  }).catch(function(){
    document.getElementById('settingsStatus').textContent='Failed to load settings';
  });
}
function saveSettings(){
  var payload={
    brightness:parseInt(document.getElementById('brightness').value,10),
    time24Hour:document.getElementById('time24Hour').checked,
    webuiDepartureCount:parseInt(document.getElementById('webuiDepartureCount').value,10)
  };
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('settingsStatus').textContent='Settings saved';
    loadSettings();
    loadStats();
    loadJson();
  }).catch(function(e){
    document.getElementById('settingsStatus').textContent='Save failed';
    console.warn(e);
  });
}
function resetSettings(){
  fetch('/api/settings/reset',{method:'POST'})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('settingsStatus').textContent='Defaults restored';
    loadSettings();
    loadStats();
    loadJson();
  }).catch(function(e){
    document.getElementById('settingsStatus').textContent='Reset failed';
    console.warn(e);
  });
}
function renderStopConfigRows(){
  var rows=document.getElementById('stopConfigRows');
  if(!Array.isArray(stopsConfig)||stopsConfig.length===0){
    rows.innerHTML='<div class="sub">No stop data</div>';
    return;
  }
  var h='';
  stopsConfig.forEach(function(s,i){
    h+='<div class="stoprow">';
    h+='<div class="field"><label for="stopId'+i+'">Stop ID</label><input id="stopId'+i+'" type="text" value="'+esc(s.id)+'" placeholder="Stop ID"></div>';
    h+='<div class="field"><label for="stopName'+i+'">Display name</label><input id="stopName'+i+'" type="text" value="'+esc(s.name)+'" placeholder="Stop name"></div>';
    h+='</div>';
  });
  rows.innerHTML=h;
}
function loadStopConfig(){
  fetch('/api/stops').then(function(r){return r.json();}).then(function(arr){
    stopsConfig=arr;
    renderStopConfigRows();
  }).catch(function(e){
    document.getElementById('stopStatus').textContent='Failed to load stops';
    console.warn(e);
  });
}
function saveStopConfig(){
  var payload=[];
  for(var i=0;i<stopsConfig.length;i++){
    payload.push({
      id:document.getElementById('stopId'+i).value.trim(),
      name:document.getElementById('stopName'+i).value.trim()
    });
  }
  fetch('/api/stops',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('stopStatus').textContent='Stops saved, refresh queued';
    loadStopConfig();
    loadStats();
    loadJson();
  }).catch(function(e){
    document.getElementById('stopStatus').textContent='Save failed';
    console.warn(e);
  });
}
function resetStopConfig(){
  fetch('/api/stops/reset',{method:'POST'})
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(){
    document.getElementById('stopStatus').textContent='Default stops restored';
    loadStopConfig();
    loadStats();
    loadJson();
  }).catch(function(e){
    document.getElementById('stopStatus').textContent='Reset failed';
    console.warn(e);
  });
}
function loadStats(){
  fetch('/api/stats').then(function(r){return r.json();}).then(function(d){
    document.getElementById('statTime').textContent=d.time+'  '+d.date;
    document.getElementById('statIp').textContent=d.ip||'--';
    document.getElementById('statWifi').textContent=(d.ssid||'--')+' / '+d.rssi+' dBm';
    document.getElementById('statUptime').textContent=fmtDuration(d.uptimeSec);
    document.getElementById('statHeap').textContent=d.freeHeap+' free / '+d.maxAllocHeap+' max';
    document.getElementById('statFetch').textContent=fmtFetchAge(d.lastFetchAgeSec)+' / poll '+d.pollIntervalSec+'s';
    document.getElementById('statRows').textContent=d.webuiDepartureCount+' of '+d.maxStoredDepartures+' stored';
    document.getElementById('statTftRows').textContent=d.tftDepartures;
    var h='';
    (d.stops||[]).forEach(function(s){
      h+='<div class="stopstat">'
        +'<span>'+esc(s.name)+'</span>'
        +'<span class="pill">'+(s.valid?'OK':'No data')+'</span>'
        +'<span class="pill">'+fmtFetchAge(s.fetchAge)+'</span>'
        +'</div>';
    });
    document.getElementById('stopStats').innerHTML=h;
  }).catch(function(e){
    console.warn(e);
  });
}
function loadJson(){
  fetch('/api/state').then(function(r){return r.json();}).then(function(d){
    document.getElementById('jsonState').textContent=JSON.stringify(d,null,2);
  }).catch(function(){
    document.getElementById('jsonState').textContent='Failed to load /api/state';
  });
}
loadSettings();
loadStopConfig();
loadStats();
loadJson();
setInterval(loadStats,10000);
setInterval(loadJson,15000);
</script>
</body></html>)rawliteral";

static void handleRoot(AsyncWebServerRequest* req) {
  req->send(200, "text/html", ROOT_HTML);
}

static void handleConfigPage(AsyncWebServerRequest* req) {
  req->send(200, "text/html", CONFIG_HTML);
}

static void handleMirror(AsyncWebServerRequest* req) {
  req->redirect("/");
}

void initWebServer() {
  server.on("/",                  HTTP_GET, handleRoot);
  server.on("/config",            HTTP_GET, handleConfigPage);
  server.on("/mirror",            HTTP_GET, handleMirror);
  server.on("/api/state",         HTTP_GET, handleApiState);
  server.on("/api/stats",         HTTP_GET, handleApiStats);
  server.on("/api/stops",         HTTP_GET, handleApiStops);
  server.on("/api/settings",      HTTP_GET, handleApiSettings);
  server.on("/api/stops",         HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr, handleApiStopsUpdate);
  server.on("/api/settings",      HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr, handleApiSettingsUpdate);
  server.on("/api/stops/reset",   HTTP_POST, [](AsyncWebServerRequest* req){
    if (!resetStopConfig() || !saveStopConfig()) {
      req->send(500, "application/json", "{\"error\":\"reset failed\"}");
      return;
    }
    s_stopRefreshRequested = true;
    req->send(200, "application/json", "{\"result\":\"reset\",\"refresh\":\"queued\"}");
  });
  server.on("/api/settings/reset", HTTP_POST, [](AsyncWebServerRequest* req){
    if (!resetUserSettings() || !saveUserSettings()) {
      req->send(500, "application/json", "{\"error\":\"reset failed\"}");
      return;
    }
    setBrightness(displayBrightness);
    s_displayRefreshRequested = true;
    req->send(200, "application/json", "{\"result\":\"reset\"}");
  });

  server.begin();
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void handleWebServer() {
}

bool consumeStopRefreshRequest() {
  bool requested = s_stopRefreshRequested;
  s_stopRefreshRequested = false;
  return requested;
}

bool consumeDisplayRefreshRequest() {
  bool requested = s_displayRefreshRequested;
  s_displayRefreshRequested = false;
  return requested;
}
