#ifndef _IOTSASLEEPPOLICY_H_
#define _IOTSASLEEPPOLICY_H_
#include <stdint.h>

// Intended to be included from iotsaController.h

//
// IotsaSleepPolicy -- the sleep/wake decision, being extracted from IotsaConfig
// and IotsaBatteryMod (cwi-dis/iotsa#106).
//
// Pure logic, no platform headers: this decides *whether* and *how* the device
// should sleep; IotsaRunmodeMod holds the actual esp_*_sleep_start() machinery
// (under IOTSA_HAS_SLEEP) and calls it from its loop(). Held by value as
// IotsaController::_sleep and reached through iotsaController.sleep() plus the
// iotsaController.{pause,resume,postpone}Sleep() / canSleep() wrappers -- those
// have callers all over the framework (iotsaInput, iotsaApiRest, iotsaBLEClient,
// ...) so the sleep-inhibit surface is always compiled even when nothing sleeps.
//
// So far this holds only the sleep-inhibit bookkeeping moved off IotsaConfig; the
// sleep config + wake-window state + decide() follow in later #106 commits.
//

class IotsaSleepPolicy {
  friend class IotsaController;
public:
  void pauseSleep()  { _pauseSleepCount++; }
  void resumeSleep() { _pauseSleepCount--; }
  // Push "no sleep before now+ms" out (ms is also bumped by
  // activityExtraWakeDuration). Returns the millis until sleep is allowed again,
  // or 0 if it already is.
  uint32_t postponeSleep(uint32_t ms);
  // True when nothing is currently inhibiting sleep. IotsaController::canSleep()
  // layers "and not in CONFIG/OTA mode" on top of this.
  bool canSleep();

  // Extra time to stay awake after any activity (a config change, a REST call,
  // ...). 0 => activity does not extend the wake window.
  uint32_t activityExtraWakeDuration = 0;

private:
  int _pauseSleepCount = 0;
  uint32_t _postponeSleepMillis = 0;
};
#endif
