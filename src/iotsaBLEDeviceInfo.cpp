#include "iotsaBLEDeviceInfo.h"

#ifdef IOTSA_WITH_BLE

IotsaBLEDeviceInfo::IotsaBLEDeviceInfo(std::string _name, std::string _address)
: name(_name),
  addressMutex(xSemaphoreCreateMutex()),
  address(_address, 0), // Public is default for address type for nimble
#ifdef IOTSA_WITHOUT_NIMBLE
  addressType(BLE_ADDR_TYPE_PUBLIC),
#endif
  addressValid(false)
{
  if (_address != "") {
    // address and addressType have already been set. Not yet shared with
    // any other task, so no need to take addressMutex here.
    addressValid = true;
  }
}

IotsaBLEDeviceInfo::~IotsaBLEDeviceInfo() {
  vSemaphoreDelete(addressMutex);
}

void IotsaBLEDeviceInfo::setKnownAddress(const std::string& _address) {
  if (_address == "") return;
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEDeviceInfo::setKnownAddress: address mutex timeout, skipped");
    return;
  }
  if (!(addressValid && address.toString() == _address)) {
    address = BLEAddress(_address, 0); // Public is default for address type for nimble
    addressValid = true;
  }
  xSemaphoreGive(addressMutex);
}

std::string IotsaBLEDeviceInfo::getAddress() {
  std::string rv = "";
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEDeviceInfo::getAddress: address mutex timeout");
    return rv;
  }
  bool valid = addressValid;
#ifdef IOTSA_WITHOUT_NIMBLE
  valid = valid && addressType == BLE_ADDR_TYPE_PUBLIC;
#endif
  if (valid) rv = address.toString();
  xSemaphoreGive(addressMutex);
  return rv;
}

bool IotsaBLEDeviceInfo::receivedAdvertisement(const BLEAdvertisedDevice& _device) {
  lastSeenAtMillis = millis();
  rssi = _device.getRSSI();
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEDeviceInfo::receivedAdvertisement: address mutex timeout, skipped");
    return false;
  }
  // Check whether the address is the same, then we don't have to add anything.
  bool sameAddress = addressValid
#ifdef IOTSA_WITHOUT_NIMBLE
    && _device.getAddressType() == addressType
#endif
    && _device.getAddress().equals(address);
  bool changed = false;
  if (!sameAddress) {
    address = _device.getAddress();
#ifdef IOTSA_WITHOUT_NIMBLE
    addressType = _device.getAddressType();
#endif
    addressValid = true;
    changed = true;
  }
  xSemaphoreGive(addressMutex);
  return changed;
}

void IotsaBLEDeviceInfo::getHandler(JsonObject& reply) {
  reply["name"] = name;
  std::string addr = getAddress();
  if (addr != "") reply["address"] = String(addr.c_str());
  if (lastSeenAtMillis != 0) {
    reply["rssi"] = rssi;
    reply["lastSeenMillisAgo"] = millis() - lastSeenAtMillis;
  }
}

#endif // IOTSA_WITH_BLE
