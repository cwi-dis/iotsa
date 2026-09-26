#include "iotsaRunmodeBLEClient.h"
#ifdef IOTSA_WITH_BLE

bool IotsaRunmodeBLEClient::getCurrentMode(uint8_t& mode) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::currentModeUUID);
  return get(svc, chr, mode);
}

bool IotsaRunmodeBLEClient::requestMode(uint8_t mode) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::requestedModeUUID);
  return set(svc, chr, mode);
}

bool IotsaRunmodeBLEClient::reboot() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::rebootUUID);
  return set(svc, chr, (uint8_t)1);
}

bool IotsaRunmodeBLEClient::promoteMode() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::promoteModeUUID);
  return set(svc, chr, (uint8_t)1);
}

bool IotsaRunmodeBLEClient::setWifiDisabled(bool disabled) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::wifiDisabledUUID);
  return set(svc, chr, (uint8_t)(disabled ? 1 : 0));
}

bool IotsaRunmodeBLEClient::identify() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::identifyUUID);
  return set(svc, chr, (uint8_t)1);
}
#endif // IOTSA_WITH_BLE
