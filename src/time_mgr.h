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

// Format a UTC epoch as local HH:MM (Sydney TZ).
// Writes into buf (must be >= 6 bytes). Returns buf.
char* formatLocalHHMM(time_t epochUTC, char* buf, size_t bufLen);

// Returns true if the given UTC epoch falls on today's date in local timezone.
bool isLocalToday(time_t epochUTC);

// Returns the current local timezone offset from UTC in seconds (e.g. +39600 for AEDT).
int getLocalTZOffset();

// Format a UTC epoch as a local 3-letter day abbreviation ("Mon", "Tue", etc.).
// Writes into buf (must be >= 4 bytes). Returns buf.
char* formatLocalDayAbbr(time_t epochUTC, char* buf, size_t bufLen);

// Formatted local time string for logs, e.g. "14:37:05".
// Pointer valid until next call.
const char* getLogTimeStr();
