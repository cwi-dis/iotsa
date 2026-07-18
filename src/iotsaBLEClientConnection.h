#ifndef _IOTSABLECLIENTCONNECTION_H_
#define _IOTSABLECLIENTCONNECTION_H_
#include "iotsa.h"
#include "iotsaBle.h"

#ifdef IOTSA_WITH_BLE

typedef std::function<void(uint8_t *, size_t)> BleNotificationCallback;

class IotsaBLEClientConnection {
  friend class IotsaBLEClientMod;
public:
  IotsaBLEClientConnection(std::string& _name, std::string _address="");
  ~IotsaBLEClientConnection();
  bool receivedAdvertisement(const BLEAdvertisedDevice& _device);
  void clearDevice();
  bool available();
  bool connect();
  void disconnect();
  bool isConnected();
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, const uint8_t *data, size_t size);
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t value);
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t value);
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t value);
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, const std::string& value);
  bool set(BLEUUID& serviceUUID, BLEUUID& charUUID, const String& value);
  bool get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t& value);
  bool get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t& value);
  bool get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t& value);
  bool get(BLEUUID& serviceUUID, BLEUUID& charUUID, std::string& value);
  bool getAsBuffer(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t **datap, size_t *sizep);
  bool getAsNotification(BLEUUID& serviceUUID, BLEUUID& charUUID, BleNotificationCallback callback);
  const std::string& getName() { return name; }
  std::string getAddress();
  // Seed a previously-persisted address, so available()/connect() work
  // before any advertisement has been received (e.g. right after boot).
  void setKnownAddress(const std::string& _address);
protected:
  std::string name;
  BLERemoteCharacteristic *_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID);
  BLEAddress address;
#ifdef IOTSA_WITHOUT_NIMBLE
  esp_ble_addr_type_t addressType;
#endif
  bool addressValid;
  BLEClient* pClient;
  const uint8_t connectionTimeoutSeconds = 6; // xxxjack should be configurable
};

#endif // IOTSA_WITH_BLE
#endif
