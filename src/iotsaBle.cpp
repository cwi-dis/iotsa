#include "iotsaBle.h"
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
#endif // IOTSA_WITH_BLE
