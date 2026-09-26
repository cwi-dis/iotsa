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
#endif // IOTSA_WITH_BLE
