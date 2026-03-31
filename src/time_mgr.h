#pragma once
#include <Arduino.h>
#include <ezTime.h>

// Initialise NTP and set timezone. Returns false if sync times out.
bool initTime();

// Formatted time string — respects TIME_24HR_DEFAULT (Phase 2: NVS override).
// Pointer valid until next call.
const char* getTimeStr();

// Formatted date string, e.g. "Tue, 1 Apr".
// Pointer valid until next call.
const char* getDateStr();

// Current UTC epoch — used by bus_api to compute minutes-until departure.
time_t getUTCNow();
