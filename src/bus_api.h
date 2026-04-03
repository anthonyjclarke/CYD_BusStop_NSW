#pragma once
#include <Arduino.h>
#include "../include/config.h"

struct Departure {
  char   route[8];        // e.g. "501"
  char   clockTime[6];    // e.g. "10:48" — local Sydney time, derived from epochUTC
  char   destination[32]; // e.g. "Gladesville - Jordan St" — from transportation.destination.name
  time_t epochUTC;        // estimated departure as UTC epoch — recalculate minutes from this
  time_t epochPlanned;    // planned departure as UTC epoch — for delay calculation
  int    minutesUntil;    // computed from epochUTC; refreshed by recalcMinutes()
  int    delaySecs;       // estimated - planned, in seconds; 0 if no estimate available
  bool   isRealtime;      // true if isRealtimeControlled — live GPS vs scheduled
  bool   valid;
};

struct StopData {
  Departure departures[DEPARTURES_PER_STOP];
  uint8_t   count;
  bool      valid;          // at least one departure returned
  bool      hasAlerts;      // true if infos[] was non-empty
  char      alertText[64];  // first alert subtitle, truncated
  uint32_t  lastFetchMs;    // millis() of last successful fetch
};

// Global stop data array — indexed by STOP_IDS / STOP_NAMES order
extern StopData stopData[STOP_COUNT];

void initBusApi();
bool fetchStop(uint8_t stopIndex);
void fetchAllStops();      // sequential fetch of all stops with INTER_REQUEST_MS gap
void recalcMinutes();      // recalculate minutesUntil from stored epoch for all stops
