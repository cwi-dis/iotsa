#include "iotsaImmediateAlertBLEServer.h"
#ifdef IOTSA_WITH_BLE

void IotsaImmediateAlertBLEServer::setup() {
  bleApi.setup(IotsaImmediateAlertBLE::serviceUUID, this);
  bleApi.addCharacteristic(
    IotsaImmediateAlertBLE::alertLevelUUID,
    bleApi.BLE_WRITE,
    NimBLE2904::FORMAT_UINT8,
    0x2700,
    "Alert Level"
    );
}

bool IotsaImmediateAlertBLEServer::blePutHandler(UUIDstring charUUID) {
  if (charUUID == IotsaImmediateAlertBLE::alertLevelUUID) {
    uint8_t level = (uint8_t)bleApi.getAsInt(IotsaImmediateAlertBLE::alertLevelUUID);
    if (callback) callback(level);
    return true;
  }
  return false;
}

bool IotsaImmediateAlertBLEServer::bleGetHandler(UUIDstring charUUID) {
  // Alert Level is write-only per the IAS spec -- nothing to read back.
  return false;
}
#endif // IOTSA_WITH_BLE
