#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name, std::string _address)
: name(_name),
  address(_address, 0), // Public is default for address type for nimble
#ifdef IOTSA_WITHOUT_NIMBLE
  addressType(BLE_ADDR_TYPE_PUBLIC),
#endif
  addressValid(false),
  pClient(nullptr)
{
  if (_address != "") {
    // address and addressType have already been set
    addressValid = true;
  }
}

IotsaBLEClientConnection::~IotsaBLEClientConnection() {
  if (pClient) {
    BLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
}

std::string IotsaBLEClientConnection::getAddress() {
  if (!addressValid
#ifdef IOTSA_WITHOUT_NIMBLE
    || addressType != BLE_ADDR_TYPE_PUBLIC
#endif
    ) return "";
  return address.toString();
}

bool IotsaBLEClientConnection::receivedAdvertisement(const BLEAdvertisedDevice& _device) {
  // Check whether the address is the same, then we don't have to add anything.
  if (addressValid
#ifdef IOTSA_WITHOUT_NIMBLE
    && _device.getAddressType() == addressType
#endif
    && _device.getAddress().equals(address)) {
    return false;
  }
  disconnect();
  address = _device.getAddress();
#ifdef IOTSA_WITHOUT_NIMBLE
  addressType = _device.getAddressType();
#endif
  addressValid = true;
  return true;
}

void IotsaBLEClientConnection::clearDevice() {
  addressValid = false;
  disconnect();
}

bool IotsaBLEClientConnection::available() {
  return addressValid;
}

bool IotsaBLEClientConnection::connect() {
  if (!addressValid) return false;
  if (pClient == nullptr) {
    pClient = BLEDevice::createClient(address);
    pClient->setConnectTimeout(connectionTimeoutSeconds);
  }
  if (pClient->isConnected()) return true;
#ifdef IOTSA_WITHOUT_NIMBLE
  bool rv = pClient->connect(address, addressType);
#else
  bool rv = pClient->connect(address, false); // Keep previously learned services
#endif
  if (!rv) clearDevice();
  return rv;
}

void IotsaBLEClientConnection::disconnect() {
  if (pClient && pClient->isConnected()) {
    pClient->disconnect();
  }
}

bool IotsaBLEClientConnection::isConnected() {
  return pClient && pClient->isConnected();
}

BLERemoteCharacteristic *IotsaBLEClientConnection::_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  BLERemoteService *service = pClient->getService(serviceUUID);
  if (service == NULL) return NULL;
  BLERemoteCharacteristic *characteristic = service->getCharacteristic(charUUID);
  return characteristic;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const uint8_t *data, size_t size) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canWrite()) return false;
  characteristic->writeValue(data, size);
  return true;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 1);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 2);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 4);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const std::string& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const String& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::getAsBuffer(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t **datap, size_t *sizep) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canRead()) return false;
  std::string value = characteristic->readValue();
  *datap = (uint8_t *)value.c_str();
  *sizep = value.length();
  return true;
}
bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint8_t)) return false;
  value = *(uint8_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint16_t)) return false;
  value = *(uint16_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint32_t)) return false;
  value = *(uint32_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, std::string& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  value = std::string((const char *)ptr, size);
  return true;
}

static BleNotificationCallback _staticCallback;

static void _staticCallbackCaller(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (_staticCallback) _staticCallback(pData, length);
}

bool IotsaBLEClientConnection::getAsNotification(BLEUUID& serviceUUID, BLEUUID& charUUID, BleNotificationCallback callback) {
  if (_staticCallback != NULL) {
    IotsaSerial.println("IotsaBLEClientConnection: only a single notification supported");
    return false;
  }
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canNotify()) return false;
  _staticCallback = callback;
  characteristic->subscribe(true, _staticCallbackCaller);
  return false;
}
#endif // IOTSA_WITH_BLE
