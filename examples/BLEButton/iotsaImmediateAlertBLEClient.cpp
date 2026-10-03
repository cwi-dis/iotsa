#include "iotsaImmediateAlertBLEClient.h"
#ifdef IOTSA_WITH_BLE

bool IotsaImmediateAlertBLEClient::setAlertLevel(IotsaImmediateAlertBLE::AlertLevel level) {
  NimBLEUUID svc(IotsaImmediateAlertBLE::serviceUUID);
  NimBLEUUID chr(IotsaImmediateAlertBLE::alertLevelUUID);
  return set(svc, chr, (uint8_t)level);
}

bool IotsaImmediateAlertBLEClient::doWork() {
  bool ok = true;
  if (ringPending) {
    ringPending = false;
    ok = ring();
  }
  // Also any queued runmode command (identify/reboot/...).
  return IotsaRunmodeBLEClient::doWork() && ok;
}

void IotsaImmediateAlertBLEClient::workAbandoned() {
  ringPending = false;
  IotsaRunmodeBLEClient::workAbandoned();
}
#endif // IOTSA_WITH_BLE
