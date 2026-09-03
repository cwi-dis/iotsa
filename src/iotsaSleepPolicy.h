#ifndef _IOTSASLEEPPOLICY_H_
#define _IOTSASLEEPPOLICY_H_
#include <stdint.h>

// Intended to be included from iotsaController.h

//
// IotsaSleepPolicy -- the sleep/wake *runtime* state and decision (cwi-dis/iotsa#106).
//
// Pure logic, no platform headers. Holds the sleep-inhibit bookkeeping (always
// compiled -- callers all over the framework) + the wake-window clock, and
// decide() answers "should the device sleep this tick, and how". It never sleeps
// itself: IotsaRunmodeMod holds the esp_*_sleep_start() machinery (under
// IOTSA_HAS_SLEEP) and executes decide()'s result. Held by value as
// IotsaController::_sleep.
//
// The persisted sleep *config* is NOT here -- it belongs to IotsaRunmodeMod,
// which persists it (sleep.cfg) and passes it to decide() as a SleepConfig
// (cwi-dis/iotsa#106 step 5b). The one exception is activityExtraWakeDuration:
// it tunes the always-compiled inhibit path (noteActivity / postponeSleep), so
// it lives here and IotsaRunmodeMod seeds it at configLoad().
//

enum IotsaSleepMode : uint8_t {
  IOTSA_SLEEP_NONE,
  IOTSA_SLEEP_DELAY,
  IOTSA_SLEEP_LIGHT,
  IOTSA_SLEEP_DEEP,
  IOTSA_SLEEP_HIBERNATE,
  _IOTSA_SLEEP_MAX
};

struct IotsaSleepDecision {
  IotsaSleepMode mode = IOTSA_SLEEP_NONE;   // IOTSA_SLEEP_NONE => stay awake
  uint32_t durationMs = 0;                  // timer-wakeup duration (0 => none set)
};

// The persisted sleep config -- owned and persisted by IotsaRunmodeMod, read by
// IotsaSleepPolicy::decide(). Not the watchdog / CPU-frequency knobs (still
// IotsaRunmodeMod members) nor activityExtraWakeDuration (on IotsaSleepPolicy).
struct IotsaSleepConfig {
  IotsaSleepMode mode = IOTSA_SLEEP_NONE;
  uint32_t sleepDuration = 0;          // sleep length (ms); 0 => no timer wakeup
  uint32_t wakeDuration = 0;           // stay awake this long after each wake (ms)
  uint32_t bootExtraWakeDuration = 0;  // extra, only on a power-on/reset wake
  bool disableSleepOnWiFi = false;     // don't sleep while the WiFi radio is up
  bool disableWiFiOnSleep = false;     // power WiFi down before sleeping
  bool disableSleepOnUSBPower = false; // don't sleep while on USB power
};

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

  // ---- sleep-inhibit bookkeeping (always compiled; callers all over the
  //      framework -- iotsaInput, iotsaApiRest, iotsaBLEClient, iotsaBLEServer) ----

  // "Someone just interacted with us" (a REST/web/BLE request, a button press,
  // config mode extended, ...). Holds sleep off for
  // max(activityExtraWakeDuration, ACTIVITY_FLOOR_MS). Replaces the old
  // postponeSleep(0) idiom.
  void noteActivity();
  void pauseSleep()  { _pauseSleepCount++; }
  void resumeSleep() { _pauseSleepCount--; }
  // A known-duration hold: no sleep before now + ms + activityExtraWakeDuration.
  void postponeSleep(uint32_t ms);
  // Millis until sleep is allowed again (0 if it already is). Read-only bar the
  // lazy self-clear.
  uint32_t millisUntilSleepAllowed();
  // True when nothing is currently inhibiting sleep. IotsaController::canSleep()
  // layers "and not in CONFIG/OTA mode" on top of this.
  bool canSleep();

  // Extra time to stay awake after any activity, on top of ACTIVITY_FLOOR_MS.
  // Persisted knob, seeded here by IotsaRunmodeMod::configLoad() -- it's consumed
  // by the inhibit path above, not by decide().
  uint32_t activityExtraWakeDuration = 0;

  // ---- wake-window state ----
  uint32_t millisAtWakeup = 0;        // millis() of the last (re)start of the wake window; 0 = not started
  bool didWakeFromSleep = false;      // false only on the power-on/reset boot

  // Should the device sleep this tick? cfg is IotsaRunmodeMod's SleepConfig;
  // onUsbPower is iotsaStatus.onUsbPower (false when there's no battery module).
  // The caller has already checked pinDisableSleep and iotsaController.canSleep().
  IotsaSleepDecision decide(const IotsaSleepConfig& cfg, bool onUsbPower);
  // (Re)start the wake-window clock -- once when the executor first runs, and
  // after every wake.
  void noteAwake();
  void noteWokeFromSleep();

private:
  int _pauseSleepCount = 0;
  uint32_t _postponeSleepMillis = 0;
};
#endif
