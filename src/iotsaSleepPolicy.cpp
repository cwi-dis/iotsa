#include "iotsa.h"
#include "iotsaSleepPolicy.h"

// Moved verbatim from IotsaConfig (cwi-dis/iotsa#106); no C++ forwarder is kept,
// the in-tree callers were renamed to iotsaController.postponeSleep() etc. in the
// same commit and downstream is swept when #106 hits develop.

uint32_t IotsaSleepPolicy::postponeSleep(uint32_t ms) {
  uint32_t noSleepBefore = millis() + ms + activityExtraWakeDuration;
  if (noSleepBefore > _postponeSleepMillis) _postponeSleepMillis = noSleepBefore;
  int32_t rv = _postponeSleepMillis - millis();
  if (rv < 2) rv = 0;
  return rv;
}

bool IotsaSleepPolicy::canSleep() {
  if (_pauseSleepCount > 0) return false;
  if (millis() > _postponeSleepMillis) _postponeSleepMillis = 0;
  return _postponeSleepMillis == 0;
}
