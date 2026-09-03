#include "iotsa.h"
#include "iotsaSleepPolicy.h"

// Moved off IotsaConfig (cwi-dis/iotsa#106); no C++ forwarder is kept, the
// in-tree callers were renamed to iotsaController.noteActivity() /
// iotsaController.postponeSleep() in the same commits and downstream is swept
// when #106 hits develop.

void IotsaSleepPolicy::noteActivity() {
  uint32_t grace = activityExtraWakeDuration;
  if (grace < ACTIVITY_FLOOR_MS) grace = ACTIVITY_FLOOR_MS;
  uint32_t noSleepBefore = millis() + grace;
  if (noSleepBefore > _postponeSleepMillis) _postponeSleepMillis = noSleepBefore;
}

void IotsaSleepPolicy::postponeSleep(uint32_t ms) {
  uint32_t noSleepBefore = millis() + ms + activityExtraWakeDuration;
  if (noSleepBefore > _postponeSleepMillis) _postponeSleepMillis = noSleepBefore;
}

uint32_t IotsaSleepPolicy::millisUntilSleepAllowed() {
  if (millis() > _postponeSleepMillis) _postponeSleepMillis = 0;
  int32_t rv = _postponeSleepMillis - millis();
  if (rv < 2) rv = 0;
  return rv;
}

bool IotsaSleepPolicy::canSleep() {
  if (_pauseSleepCount > 0) return false;
  if (millis() > _postponeSleepMillis) _postponeSleepMillis = 0;
  return _postponeSleepMillis == 0;
}
