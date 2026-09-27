#include "iotsaImmediateAlertBLEClient.h"
#ifdef IOTSA_WITH_BLE

bool IotsaImmediateAlertBLEClient::setAlertLevel(IotsaImmediateAlertBLE::AlertLevel level) {
  NimBLEUUID svc(IotsaImmediateAlertBLE::serviceUUID);
  NimBLEUUID chr(IotsaImmediateAlertBLE::alertLevelUUID);
  return set(svc, chr, (uint8_t)level);
}
#endif // IOTSA_WITH_BLE
