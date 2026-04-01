#pragma once
#include <Arduino.h>
#include "../include/config.h"

struct Departure {
  char   route[8];        // e.g. "501"
  char   clockTime[6];    // e.g. "10:48" — local time from API string
  time_t epochUTC;        // departure time as UTC epoch — recalculate minutes from this
  int    minutesUntil;    // computed from epochUTC; refreshed by recalcMinutes()
  bool   valid;
};

struct StopData {
  Departure departures[DEPARTURES_PER_STOP];
  uint8_t   count;
  bool      valid;        // at least one departure returned
  uint32_t  lastFetchMs;  // millis() of last successful fetch
};

// Global stop data array — indexed by STOP_IDS / STOP_NAMES order
extern StopData stopData[STOP_COUNT];

void initBusApi();
bool fetchStop(uint8_t stopIndex);
void fetchAllStops();      // sequential fetch of all stops with INTER_REQUEST_MS gap
void recalcMinutes();      // recalculate minutesUntil from stored epoch for all stops
