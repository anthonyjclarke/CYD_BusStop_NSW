#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

static Timezone myTZ;
static char     s_timeBuffer[12];
static char     s_dateBuffer[20];

bool initTime() {
  DBG_INFO("Waiting for NTP sync (30s timeout)...");
  waitForSync(30);

  if (timeStatus() != timeSet) {
    DBG_ERROR("NTP sync failed — time unavailable");
    return false;
  }

  myTZ.setLocation(F(TIMEZONE));
  DBG_INFO("Time synced: %s", myTZ.dateTime().c_str());
  return true;
}

const char* getTimeStr() {
  // Phase 2: read 12/24hr preference from NVS; hardcoded default for now
  String t = TIME_24HR_DEFAULT
    ? myTZ.dateTime("H:i")      // 14:35
    : myTZ.dateTime("g:i A");   // 2:35 PM

  strncpy(s_timeBuffer, t.c_str(), sizeof(s_timeBuffer) - 1);
  s_timeBuffer[sizeof(s_timeBuffer) - 1] = '\0';
  return s_timeBuffer;
}

const char* getDateStr() {
  String d = myTZ.dateTime("D, j M");  // "Tue, 1 Apr"
  strncpy(s_dateBuffer, d.c_str(), sizeof(s_dateBuffer) - 1);
  s_dateBuffer[sizeof(s_dateBuffer) - 1] = '\0';
  return s_dateBuffer;
}

time_t getUTCNow() {
  return now();  // ezTime now() returns UTC epoch
}
