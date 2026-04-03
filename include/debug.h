#pragma once
#include <Arduino.h>

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

// Returns a timestamp prefix for debug logs.
// After time sync: local wall-clock "HH:MM:SS".
// Before time sync: uptime fallback "t+12.345s".
const char* dbgTimestamp();

#define _DBG_TS()  Serial.printf("[%s] ", dbgTimestamp())

#if DEBUG_LEVEL >= 1
  #define DBG_ERROR(fmt, ...)   do { _DBG_TS(); Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define DBG_ERROR(fmt, ...)
#endif

#if DEBUG_LEVEL >= 2
  #define DBG_WARN(fmt, ...)    do { _DBG_TS(); Serial.printf("[WARN]  " fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define DBG_WARN(fmt, ...)
#endif

#if DEBUG_LEVEL >= 3
  #define DBG_INFO(fmt, ...)    do { _DBG_TS(); Serial.printf("[INFO]  " fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define DBG_INFO(fmt, ...)
#endif

#if DEBUG_LEVEL >= 4
  #define DBG_VERBOSE(fmt, ...) do { _DBG_TS(); Serial.printf("[VERB]  " fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define DBG_VERBOSE(fmt, ...)
#endif
