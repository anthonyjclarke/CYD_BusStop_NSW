#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

#include "../include/config.h"
#include "../include/debug.h"
#include "../include/secrets.h"

#include "display.h"
#include "time_mgr.h"
#include "bus_api.h"
#include "web_server.h"

static uint32_t s_lastPoll         = 0;
static uint32_t s_lastClockUpdate  = 0;
static uint32_t s_lastPanelRefresh = 0;
static char     s_lastFetchStr[6]  = "--:--";  // local HH:MM of last API fetch

static void performBusRefresh() {
  fetchAllStops();
  formatLocalHHMM(getUTCNow(), s_lastFetchStr, sizeof(s_lastFetchStr));
  drawAllStops();
  drawLastUpdated(s_lastFetchStr);
}

// ---------------------------------------------------------------------------
// Init helpers
// ---------------------------------------------------------------------------

static void initWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);  // 3 min AP timeout — restarts if no connection

  // Seed credentials from secrets.h if provided — pre-fills the portal form
  if (strlen(SECRET_WIFI_SSID) > 0) {
    wm.setConfigPortalTimeout(0);  // wait indefinitely when seeds are set
    WiFiManagerParameter p_ssid("ssid", "SSID", SECRET_WIFI_SSID, 64);
    WiFiManagerParameter p_pass("pass", "Password", SECRET_WIFI_PASS, 64);
    wm.addParameter(&p_ssid);
    wm.addParameter(&p_pass);
  }

  if (!wm.autoConnect(WIFI_AP_NAME)) {
    DBG_ERROR("WiFiManager failed to connect — restarting");
    delay(1000);
    ESP.restart();
  }

  DBG_INFO("WiFi connected — IP: %s", WiFi.localIP().toString().c_str());
}

static void initOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    DBG_INFO("OTA: starting update");
    drawStatusBar("OTA Update...", TFT_CYAN);
  });

  ArduinoOTA.onEnd([]() {
    DBG_INFO("OTA: complete — rebooting");
    drawStatusBar("OTA Done. Rebooting...", TFT_GREEN);
  });

  ArduinoOTA.onError([](ota_error_t e) {
    DBG_ERROR("OTA error: %u", e);
    drawStatusBar("OTA Error!", TFT_RED);
  });

  ArduinoOTA.begin();
  DBG_INFO("ArduinoOTA ready — hostname: %s", OTA_HOSTNAME);
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  DBG_INFO("=== CYD_BusStop starting ===");

  initDisplay();

  drawStatusBar("Connecting WiFi...", TFT_YELLOW);
  initWiFi();

  drawStatusBar("Syncing time...", TFT_YELLOW);
  initTime();

  initStopConfig();
  initBusApi();
  initWebServer();
  initOTA();

  drawStatusBar("Fetching buses...", TFT_YELLOW);
  performBusRefresh();

  // Initial full draw
  drawHeader(getTimeStr(), getDateStr());
  drawLastUpdated(s_lastFetchStr);

  s_lastPoll        = millis();
  s_lastClockUpdate = millis();

  DBG_INFO("Free heap: %u, maxBlk: %u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  DBG_INFO("=== Init complete ===");
}

void loop() {
  ArduinoOTA.handle();
  handleWebServer();

  uint32_t now = millis();

  // Update clock header every second
  if (now - s_lastClockUpdate >= 1000UL) {
    s_lastClockUpdate = now;
    events();  // ezTime internal tick — keeps NTP refreshed
    drawHeader(getTimeStr(), getDateStr());
  }

  // Recalculate minutes + redraw panels every 15s (no API call)
  if (now - s_lastPanelRefresh >= 15000UL) {
    s_lastPanelRefresh = now;
    recalcMinutes();
    drawAllStops();
    drawLastUpdated(s_lastFetchStr);
  }

  // WebUI stop edits queue a refresh so the async request task stays non-blocking.
  if (consumeStopRefreshRequest()) {
    s_lastPoll = now;
    s_lastPanelRefresh = now;
    performBusRefresh();
  }

  // Full bus API refresh on poll interval
  if (now - s_lastPoll >= POLL_INTERVAL_MS) {
    s_lastPoll         = now;
    s_lastPanelRefresh = now;  // reset so we don't double-draw
    performBusRefresh();
  }
}
