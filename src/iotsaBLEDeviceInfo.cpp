#include "iotsaBLEDeviceInfo.h"

#ifdef IOTSA_WITH_BLE

IotsaBLEDeviceInfo::IotsaBLEDeviceInfo(std::string _name, std::string _bleAddress)
: bleName(_name),
  bleAddressMutex(xSemaphoreCreateMutex()),
  bleAddress(_bleAddress, 0), // Public is default for bleAddress type for nimble
  bleAddressValid(false)
{
  if (_bleAddress != "") {
    // bleAddress and bleAddressType have already been set. Not yet shared with
    // any other task, so no need to take bleAddressMutex here.
    bleAddressValid = true;
  }
}

IotsaBLEDeviceInfo::~IotsaBLEDeviceInfo() {
  vSemaphoreDelete(bleAddressMutex);
}

void IotsaBLEDeviceInfo::setKnownAddress(const std::string& _bleAddress) {
  if (_bleAddress == "") return;
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IOTSA_LOG("iotsaBLEClient", "setKnownAddress: bleAddress mutex timeout, skipped");
    return;
  }
  if (!(bleAddressValid && bleAddress.toString() == _bleAddress)) {
    bleAddress = NimBLEAddress(_bleAddress, 0); // Public is default for bleAddress type for nimble
    bleAddressValid = true;
  }
  xSemaphoreGive(bleAddressMutex);
}

std::string IotsaBLEDeviceInfo::getAddress() {
  std::string rv = "";
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IOTSA_LOG("iotsaBLEClient", "getAddress: bleAddress mutex timeout");
    return rv;
  }
  bool valid = bleAddressValid;
  if (valid) rv = bleAddress.toString();
  xSemaphoreGive(bleAddressMutex);
  return rv;
}

bool IotsaBLEDeviceInfo::receivedAdvertisement(const NimBLEAdvertisedDevice& _device) {
  lastSeenAtMillis = millis();
  rssi = _device.getRSSI();
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IOTSA_LOG("iotsaBLEClient", "receivedAdvertisement: bleAddress mutex timeout, skipped");
    return false;
  }
  // Check whether the bleAddress is the same, then we don't have to add anything.
  bool sameAddress = bleAddressValid && _device.getAddress().equals(bleAddress);
  bool changed = false;
  if (!sameAddress) {
    bleAddress = _device.getAddress();
    bleAddressValid = true;
    changed = true;
  }
  xSemaphoreGive(bleAddressMutex);
  return changed;
}

void IotsaBLEDeviceInfo::getHandler(IotsaApiContext& ctx) {
  ctx.show("name", bleName);
  std::string addr = getAddress();
  if (addr != "") ctx.show("address", String(addr.c_str())); // REST field name unchanged by the bleAddress rename -- wire API, not internal naming
  if (lastSeenAtMillis != 0) {
    // rssi only comes from advertisements; a device can also be seen by
    // connecting to it (IotsaBLEClientDevice), then there's no rssi yet.
    if (rssi != 0) ctx.show("rssi", rssi);
    ctx.show("lastSeenMillisAgo", millis() - lastSeenAtMillis);
  }
}

#endif // IOTSA_WITH_BLE
