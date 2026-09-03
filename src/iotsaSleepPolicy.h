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
// iotsaController.noteActivity() / {pause,resume,postpone}Sleep() / canSleep()
// wrappers -- those have callers all over the framework (iotsaInput, iotsaApiRest,
// iotsaBLEClient, ...) so the sleep-inhibit surface is always compiled even when
// nothing sleeps.
//
// So far this holds only the sleep-inhibit bookkeeping moved off IotsaConfig; the
// sleep config + wake-window state + decide() follow in later #106 commits.
//

class IotsaSleepPolicy {
  friend class IotsaController;
public:
  // Minimum grace after any activity, even when activityExtraWakeDuration is 0 --
  // so a burst of REST/BLE requests can't be sliced mid-conversation.
  static constexpr uint32_t ACTIVITY_FLOOR_MS = 250;
  // A known-duration hold: after asking the WiFi radio to power down, don't
  // sleep until it has plausibly finished.
  static constexpr uint32_t WIFI_SHUTDOWN_GRACE_MS = 2000;
  // Margin added to a BLE scan's own duration before sleep is allowed again.
  static constexpr uint32_t SCAN_COMPLETION_MARGIN_MS = 1000;

  // "Someone just interacted with us" (a REST/web/BLE request, a button press,
  // config mode extended, ...). Holds sleep off for
  // max(activityExtraWakeDuration, ACTIVITY_FLOOR_MS). Replaces the old
  // postponeSleep(0) idiom.
  void noteActivity();

  void pauseSleep()  { _pauseSleepCount++; }
  void resumeSleep() { _pauseSleepCount--; }
  // A known-duration hold: no sleep before now + ms + activityExtraWakeDuration.
  // For "activity just happened" use noteActivity(); for "how long until sleep is
  // allowed" use millisUntilSleepAllowed().
  void postponeSleep(uint32_t ms);
  // Millis until sleep is allowed again (0 if it already is). Read-only bar the
  // lazy self-clear.
  uint32_t millisUntilSleepAllowed();
  // True when nothing is currently inhibiting sleep. IotsaController::canSleep()
  // layers "and not in CONFIG/OTA mode" on top of this.
  bool canSleep();

  // Extra time to stay awake after any activity, on top of ACTIVITY_FLOOR_MS.
  uint32_t activityExtraWakeDuration = 0;

private:
  int _pauseSleepCount = 0;
  uint32_t _postponeSleepMillis = 0;
};
#endif
