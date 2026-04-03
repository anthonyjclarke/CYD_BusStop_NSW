#include "time_mgr.h"
#include "../include/config.h"
#include "../include/debug.h"

static Timezone myTZ;
static char     s_timeBuffer[12];
static char     s_dateBuffer[20];
static char     s_logTimeBuffer[10];

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

char* formatLocalHHMM(time_t epochUTC, char* buf, size_t bufLen) {
  String t = myTZ.dateTime(epochUTC, UTC_TIME, "H:i");
  strncpy(buf, t.c_str(), bufLen - 1);
  buf[bufLen - 1] = '\0';
  return buf;
}

int getLocalTZOffset() {
  return myTZ.getOffset() * 60;  // ezTime getOffset() returns minutes
}

bool isLocalToday(time_t epochUTC) {
  // Compare local date of the given epoch with today's local date
  String todayDate = myTZ.dateTime("Ymd");          // e.g. "20260402"
  String depDate   = myTZ.dateTime(epochUTC, UTC_TIME, "Ymd");
  return todayDate == depDate;
}

char* formatLocalDayAbbr(time_t epochUTC, char* buf, size_t bufLen) {
  String d = myTZ.dateTime(epochUTC, UTC_TIME, "D");  // "Mon", "Tue", etc.
  strncpy(buf, d.c_str(), bufLen - 1);
  buf[bufLen - 1] = '\0';
  return buf;
}

const char* getLogTimeStr() {
  String t = myTZ.dateTime("H:i:s");
  strncpy(s_logTimeBuffer, t.c_str(), sizeof(s_logTimeBuffer) - 1);
  s_logTimeBuffer[sizeof(s_logTimeBuffer) - 1] = '\0';
  return s_logTimeBuffer;
}
