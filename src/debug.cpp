#include "../include/debug.h"
#include "time_mgr.h"

const char* dbgTimestamp() {
  static char buf[16];

  if (getUTCNow() >= 946684800) {  // 2000-01-01 00:00:00 UTC
    strncpy(buf, getLogTimeStr(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  } else {
    snprintf(buf, sizeof(buf), "t+%5.3fs", millis() / 1000.0);
  }

  return buf;
}
