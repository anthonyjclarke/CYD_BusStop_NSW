#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Stop configuration
// ---------------------------------------------------------------------------
constexpr uint8_t     STOP_COUNT          = 4;
constexpr uint8_t     DEPARTURES_PER_STOP = 3;

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

void initStopConfig();

bool setStopConfig(uint8_t idx, const char* stopId, const char* stopName);

bool saveStopConfig();

bool loadStopConfig();

bool resetStopConfig();

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
constexpr const char* WIFI_AP_NAME = "CYD-BusStop";

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
