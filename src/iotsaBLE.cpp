#include "iotsaBLE.h"
#ifdef IOTSA_WITH_BLE

static bool s_initialized = false;

void iotsaBLE_ensureInitialized() {
  if (s_initialized) return;
  iotsaConfig.ensureConfigLoaded();
  NimBLEDevice::init(iotsaConfig.hostName.c_str());
  s_initialized = true;
}

void iotsaBLE_notifyAdvertisingStateChanged(bool active) {
  IotsaSerial.printf("iotsaBLE: advertising %s\n", active ? "started" : "stopped");
}

void iotsaBLE_notifyScanningStateChanged(bool active) {
  IotsaSerial.printf("iotsaBLE: scanning %s\n", active ? "started" : "stopped");
}

uint32_t IotsaBLERadioArbiter::s_serverReservedUntilMillis = 0;
bool IotsaBLERadioArbiter::s_holdOffNewBLEWork = false;

void IotsaBLERadioArbiter::reserveConnectionForServer(uint32_t graceMs) {
  uint32_t until = millis() + graceMs;
  if (until > s_serverReservedUntilMillis) s_serverReservedUntilMillis = until;
}

bool IotsaBLERadioArbiter::serverReservationActive() {
  return millis() < s_serverReservedUntilMillis;
}

void IotsaBLERadioArbiter::holdOffNewWork(bool hold) {
  s_holdOffNewBLEWork = hold;
}

bool IotsaBLERadioArbiter::newWorkHeldOff() {
  return s_holdOffNewBLEWork;
}

std::atomic<uint8_t> IotsaBLERadioArbiter::s_activity{ACTIVITY_NONE};
volatile uint32_t IotsaBLERadioArbiter::s_scanEndedAtMillis = 0;

bool IotsaBLERadioArbiter::canBeginScan() {
  if (s_activity.load() != ACTIVITY_NONE) return false;
  if (s_holdOffNewBLEWork) return false;
  return true;
}

bool IotsaBLERadioArbiter::tryBeginScan() {
  if (!canBeginScan()) return false;
  uint8_t expected = ACTIVITY_NONE;
  return s_activity.compare_exchange_strong(expected, ACTIVITY_SCAN);
}

bool IotsaBLERadioArbiter::canBeginConnect(uint32_t settleMs) {
  if (s_activity.load() != ACTIVITY_NONE) return false;
  if (millis() - s_scanEndedAtMillis < settleMs) return false;
  if (s_holdOffNewBLEWork) return false;
  // NimBLEDevice's client pool and NimBLEServer's peers share one
  // NIMBLE_MAX_CONNECTIONS link budget: while the server role has seen
  // recent activity, keep one slot for it (confirmed needed live on
  // lissabonController, 2026-09-25, 5 dimmers competing for 3 slots).
  if (serverReservationActive() && NimBLEDevice::getCreatedClientCount() >= (size_t)(NIMBLE_MAX_CONNECTIONS - 1)) {
    return false;
  }
  return true;
}

bool IotsaBLERadioArbiter::tryBeginConnect(uint32_t settleMs) {
  if (!canBeginConnect(settleMs)) return false;
  uint8_t expected = ACTIVITY_NONE;
  return s_activity.compare_exchange_strong(expected, ACTIVITY_CONNECT);
}

void IotsaBLERadioArbiter::endActivity(Activity activity) {
  uint8_t expected = activity;
  if (!s_activity.compare_exchange_strong(expected, ACTIVITY_NONE)) {
    IotsaSerial.printf("IotsaBLERadioArbiter: endActivity(%d) but activity is %d\n", (int)activity, (int)expected);
    return;
  }
  if (activity == ACTIVITY_SCAN) s_scanEndedAtMillis = millis();
}

uint8_t IotsaBLERadioArbiter::s_advertisingPauseReasons = 0;
IotsaBLERadioArbiter::AdvertisingReconciler IotsaBLERadioArbiter::s_advertisingReconciler = nullptr;

void IotsaBLERadioArbiter::pauseAdvertising(AdvertisingPauseReason reason) {
  s_advertisingPauseReasons |= reason;
  if (s_advertisingReconciler) s_advertisingReconciler(0);
}

void IotsaBLERadioArbiter::resumeAdvertising(AdvertisingPauseReason reason, uint32_t durationMs) {
  s_advertisingPauseReasons &= ~reason;
  if (s_advertisingReconciler) s_advertisingReconciler(durationMs);
}
#endif // IOTSA_WITH_BLE
