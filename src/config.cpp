#include "../include/config.h"
#include "../include/debug.h"

#include <Preferences.h>
#include <string.h>

char stopIds[STOP_COUNT][STOP_ID_MAX];
char stopNames[STOP_COUNT][STOP_NAME_MAX];

static const char* PREF_NAMESPACE = "busstop";
static const char* PREF_COUNT_KEY = "count";
static const char* PREF_SIG_KEY   = "defsSig";

static uint32_t hashBytes(uint32_t hash, const char* s) {
  while (s && *s) {
    hash ^= (uint8_t)*s++;
    hash *= 16777619u;
  }
  return hash;
}

static uint32_t compiledDefaultsSignature() {
  uint32_t hash = 2166136261u;  // FNV-1a
  hash ^= STOP_COUNT;
  hash *= 16777619u;

  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    hash = hashBytes(hash, STOP_IDS_DEFAULT[i]);
    hash ^= 0xffu;
    hash *= 16777619u;
    hash = hashBytes(hash, STOP_NAMES_DEFAULT[i]);
    hash ^= 0x00u;
    hash *= 16777619u;
  }

  return hash;
}

static String stopIdKey(uint8_t idx) {
  return String("id") + idx;
}

static String stopNameKey(uint8_t idx) {
  return String("name") + idx;
}

void initStopConfig() {
  if (!loadStopConfig()) {
    DBG_INFO("Stop config: no saved config, using defaults");
    resetStopConfig();
    if (!saveStopConfig()) {
      DBG_WARN("Stop config: save failed after reset");
    }
  } else {
    DBG_INFO("Stop config: loaded from NVS");
  }
}

bool setStopConfig(uint8_t idx, const char* stopId, const char* stopName) {
  if (idx >= STOP_COUNT || stopId == nullptr || stopName == nullptr) {
    return false;
  }
  if (strlen(stopId) >= STOP_ID_MAX || strlen(stopName) >= STOP_NAME_MAX) {
    return false;
  }

  strncpy(stopIds[idx], stopId, STOP_ID_MAX - 1);
  stopIds[idx][STOP_ID_MAX - 1] = '\0';

  strncpy(stopNames[idx], stopName, STOP_NAME_MAX - 1);
  stopNames[idx][STOP_NAME_MAX - 1] = '\0';

  return true;
}

bool resetStopConfig() {
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    if (!setStopConfig(i, STOP_IDS_DEFAULT[i], STOP_NAMES_DEFAULT[i])) {
      return false;
    }
  }
  return true;
}

bool saveStopConfig() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    DBG_ERROR("Stop config: prefs.begin failed");
    return false;
  }

  prefs.putUChar(PREF_COUNT_KEY, STOP_COUNT);
  prefs.putUInt(PREF_SIG_KEY, compiledDefaultsSignature());

  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    prefs.putString(stopIdKey(i).c_str(), stopIds[i]);
    prefs.putString(stopNameKey(i).c_str(), stopNames[i]);
  }

  prefs.end();
  DBG_INFO("Stop config: saved %d entries", STOP_COUNT);
  return true;
}

bool loadStopConfig() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    DBG_ERROR("Stop config: prefs.begin(readonly) failed");
    return false;
  }

  uint8_t count = prefs.getUChar(PREF_COUNT_KEY, 0);
  if (count != STOP_COUNT) {
    prefs.end();
    return false;
  }

  uint32_t savedSig = prefs.getUInt(PREF_SIG_KEY, 0);
  uint32_t currentSig = compiledDefaultsSignature();
  if (savedSig != currentSig) {
    prefs.end();
    DBG_INFO("Stop config: compiled defaults changed, reapplying defaults");
    return false;
  }

  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    String id = prefs.getString(stopIdKey(i).c_str(), "");
    String name = prefs.getString(stopNameKey(i).c_str(), "");
    if (id.length() == 0 || name.length() == 0) {
      prefs.end();
      return false;
    }

    if (!setStopConfig(i, id.c_str(), name.c_str())) {
      prefs.end();
      return false;
    }
  }

  prefs.end();
  return true;
}
