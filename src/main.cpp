#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <esp_ota_ops.h>

#include "../include/config.h"
#include "../include/debug.h"

#include "display.h"
#include "time_mgr.h"
#include "bus_api.h"
#include "web_server.h"
#include "network/improv_setup.h"

static uint32_t s_lastPoll         = 0;
static uint32_t s_lastClockUpdate  = 0;
static uint32_t s_lastPanelRefresh = 0;
static char     s_lastFetchStr[6]  = "--:--";  // local HH:MM of last API fetch

// Footer: last-fetch time, or where to fix a missing/rejected API key.
static void drawFooter() {
  const char* st = apiKeyStatus();
  if (strcmp(st, "ok") == 0) {
    drawLastUpdated(s_lastFetchStr);
    return;
  }
  char msg[48];
  snprintf(msg, sizeof(msg), "%s - %s/config",
           strcmp(st, "missing") == 0 ? "No API key" : "API key rejected",
           WiFi.localIP().toString().c_str());
  drawStatusBar(msg, TFT_ORANGE);
}

static void performBusRefresh() {
  fetchAllStops();
  if (strcmp(apiKeyStatus(), "ok") == 0) {
    formatLocalHHMM(getUTCNow(), s_lastFetchStr, sizeof(s_lastFetchStr));
  }
  drawAllStops();
  drawFooter();
}

// ---------------------------------------------------------------------------
// Init helpers
// ---------------------------------------------------------------------------

static void initWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);  // 3 min AP timeout — restarts if no connection

  // TfNSW key on the portal's WiFi page. Never pre-filled; saved only if entered.
  WiFiManagerParameter p_key("apikey", "TfNSW API key (or set later on /config)",
                             "", API_KEY_MAX - 1, "type='password'");
  wm.addParameter(&p_key);

#if IMPROV_SETUP_ENABLED
  wm.setConfigPortalBlocking(false);
#endif
  bool connected = wm.autoConnect(AP_NAME);
#if IMPROV_SETUP_ENABLED
  while (!connected && wm.getConfigPortalActive()) {
    if (wm.process()) { connected = true; break; }
    improvTick();  // restarts once Improv credentials connect
    delay(5);
  }
#endif

  if (strlen(p_key.getValue()) > 0 && !setApiKey(p_key.getValue())) {
    DBG_WARN("API key from portal rejected (invalid characters or too long)");
  }

  if (!connected) {
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
  improvBegin();
  DBG_INFO("=== %s v%s starting ===", PROJECT_NAME, FIRMWARE_VERSION);
  DBG_INFO("Running from %s", esp_ota_get_running_partition()->label);

  initDisplay();

  drawStatusBar("Connecting WiFi...", TFT_YELLOW);
  initWiFi();

  drawStatusBar("Syncing time...", TFT_YELLOW);
  initTime();

  initStopConfig();
  initUserSettings();
  initApiKey();
  setBrightness(displayBrightness);
  initBusApi();
  initWebServer();
  initOTA();

  drawStatusBar("Fetching buses...", TFT_YELLOW);
  performBusRefresh();

  // Initial full draw
  drawHeader(getTimeStr(), getDateStr());
  drawFooter();

  s_lastPoll        = millis();
  s_lastClockUpdate = millis();

  DBG_INFO("Free heap: %u, maxBlk: %u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  DBG_INFO("=== Init complete ===");
}

void loop() {
  improvTick();
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
    drawFooter();
  }

  // WebUI stop edits queue a refresh so the async request task stays non-blocking.
  if (consumeStopRefreshRequest()) {
    s_lastPoll = now;
    s_lastPanelRefresh = now;
    performBusRefresh();
  }

  if (consumeDisplayRefreshRequest()) {
    drawHeader(getTimeStr(), getDateStr());
    drawAllStops();
    drawFooter();
  }

  // Full bus API refresh on poll interval
  if (now - s_lastPoll >= POLL_INTERVAL_MS) {
    s_lastPoll         = now;
    s_lastPanelRefresh = now;  // reset so we don't double-draw
    performBusRefresh();
  }
}
