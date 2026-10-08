#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Firmware identity — read by the web installer (manifest + Improv)
// ---------------------------------------------------------------------------
#define FIRMWARE_VERSION "0.4.0-dev"
#define PROJECT_NAME     "CYD_BusStop_NSW"  // frozen: Improv + manifest name
#define PROJECT_REPO_URL "https://github.com/anthonyjclarke/CYD_BusStop_NSW"

// ---------------------------------------------------------------------------
// Local secrets — optional. CI and installer images build without secrets.h.
// SECRET_TFNSW_API_KEY, if set, seeds NVS once on a board with no stored key.
// ---------------------------------------------------------------------------
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef SECRET_TFNSW_API_KEY
#define SECRET_TFNSW_API_KEY ""
#endif

// ---------------------------------------------------------------------------
// Stop configuration
// ---------------------------------------------------------------------------
constexpr uint8_t     STOP_COUNT          = 4;
constexpr uint8_t     TFT_DEPARTURES_PER_STOP   = 3;
constexpr uint8_t     MAX_STORED_DEPARTURES     = 8;
constexpr uint8_t     WEBUI_DEPARTURES_DEFAULT  = 3;

constexpr uint8_t STOP_ID_MAX   = 16;
constexpr uint8_t STOP_NAME_MAX = 24;

constexpr const char* const STOP_IDS_DEFAULT[STOP_COUNT] = {
  "2112130",  // To Gladesville
  "2112131",  // To Meadowbank Stn
  "211267",   // End of Small St
  "211271"    // To Macquarie Park
};

constexpr const char* const STOP_NAMES_DEFAULT[STOP_COUNT] = {
  "To Gladesville",
  "To Meadowbank Stn",
  "End of Small St",
  "To Macquarie Park"
};

extern char stopIds[STOP_COUNT][STOP_ID_MAX];
extern char stopNames[STOP_COUNT][STOP_NAME_MAX];
extern uint8_t displayBrightness;
extern bool    time24Hour;
extern uint8_t webuiDepartureCount;

void initStopConfig();
void initUserSettings();

bool setStopConfig(uint8_t idx, const char* stopId, const char* stopName);
bool setDisplayBrightnessSetting(uint8_t brightness);
bool setTime24HourSetting(bool enabled);
bool setWebuiDepartureCountSetting(uint8_t count);

bool saveStopConfig();
bool saveUserSettings();

bool loadStopConfig();
bool loadUserSettings();

bool resetStopConfig();
bool resetUserSettings();

// ---------------------------------------------------------------------------
// TfNSW API
// ---------------------------------------------------------------------------
constexpr const char* TFNSW_API_HOST = "api.transport.nsw.gov.au";
constexpr const char* TFNSW_API_BASE =
  "https://api.transport.nsw.gov.au/v1/tp/departure_mon"
  "?outputFormat=rapidJSON"
  "&coordOutputFormat=EPSG:4326"
  "&mode=direct"
  "&type_dm=stop"
  "&departureMonitorMacro=true"
  "&TfNSWDM=true"
  "&version=10.2.1.42"
  "&depArr=dep"
  "&limit=6";   // cap results — keeps response ~5KB vs ~60KB unbounded

constexpr uint32_t POLL_INTERVAL_MS    = 60000;   // 60 s between full refresh cycles
constexpr uint32_t INTER_REQUEST_MS    = 500;     // gap between sequential stop requests

// ---------------------------------------------------------------------------
// WiFi / provisioning
// ---------------------------------------------------------------------------
#define AP_NAME "CYD-BusStop"
constexpr const char* WIFI_AP_NAME = AP_NAME;

#define IMPROV_SETUP_ENABLED 1
#define IMPROV_DEVICE_PREFIX "BusStop"

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
constexpr const char* NTP_HOST   = "pool.ntp.org";  // renamed — ezTime defines NTP_SERVER as a macro
constexpr const char* TIMEZONE   = "Australia/Sydney";

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
constexpr uint8_t BRIGHTNESS_DEFAULT = 200;  // 0–255
constexpr bool    TIME_24HR_DEFAULT   = false;

// Layout — landscape 320×240
constexpr int HEADER_H   = 28;   // px — time + date bar
constexpr int PANEL_W    = 160;  // px — each stop panel width
constexpr int PANEL_H    = 106;  // px — each stop panel height

// ---------------------------------------------------------------------------
// OTA / network
// ---------------------------------------------------------------------------
constexpr const char* OTA_HOSTNAME = "cyd-busstop";
constexpr uint16_t    WEB_PORT     = 80;
