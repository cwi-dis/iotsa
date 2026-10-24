#include "iotsa.h"
#include "iotsaSleepPolicy.h"

// Moved off IotsaConfig + IotsaBatteryMod (cwi-dis/iotsa#106); no C++ forwarders
// are kept, the in-tree callers were renamed to iotsaController.* in the same
// commits and downstream is swept when #106 hits develop.

// ---- sleep-inhibit bookkeeping ----

void IotsaSleepPolicy::noteActivity() {
  uint32_t grace = activityExtraWakeDuration;
  if (grace < ACTIVITY_FLOOR_MS) grace = ACTIVITY_FLOOR_MS;
  _noSleepBefore.armAtLeast(grace);
}

void IotsaSleepPolicy::postponeSleep(uint32_t ms) {
  _noSleepBefore.armAtLeast(ms + activityExtraWakeDuration);
}

uint32_t IotsaSleepPolicy::millisUntilSleepAllowed() {
  uint32_t rv = _noSleepBefore.remainingMillis();
  return rv < 2 ? 0 : rv;
}

bool IotsaSleepPolicy::canSleep() {
  if (_pauseSleepCount > 0) return false;
  return !_noSleepBefore.pending();
}

// ---- wake-window + decision ----

void IotsaSleepPolicy::noteAwake() {
  millisAtWakeup = millis();
}

void IotsaSleepPolicy::noteWokeFromSleep() {
  millisAtWakeup = millis();
  didWakeFromSleep = true;
}

IotsaSleepDecision IotsaSleepPolicy::decide(const IotsaSleepConfig& cfg, bool onUsbPower) {
  IotsaSleepDecision d;
  if (cfg.mode == IOTSA_SLEEP_NONE) return d;
  uint32_t curWakeDuration = cfg.wakeDuration;
  if (!didWakeFromSleep) curWakeDuration += cfg.bootExtraWakeDuration;
  if (curWakeDuration == 0) return d;
  if (millis() - millisAtWakeup <= curWakeDuration) return d;
  // Reasons not to sleep even though the wake window has elapsed. The caller has
  // already checked pinDisableSleep (hardware) and iotsaController.canSleep()
  // (inhibits + maintenance mode).
  if (cfg.disableSleepOnWiFi && iotsaStatus.wifiEnabled) return d;
  if (cfg.disableSleepOnUSBPower && onUsbPower) return d;
  d.mode = cfg.mode;
  d.durationMs = cfg.sleepDuration;
  return d;
}
