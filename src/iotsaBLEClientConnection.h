#ifndef _IOTSABLECLIENTCONNECTION_H_
#define _IOTSABLECLIENTCONNECTION_H_
#include "iotsa.h"
#include "iotsaBle.h"

#ifdef IOTSA_WITH_BLE

#include <freertos/semphr.h>

typedef std::function<void(uint8_t *, size_t)> BleNotificationCallback;

class IotsaBLEClientMod;

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
  // millis() timestamp of the last time we saw an advertisement (or connected
  // to) this device, regardless of whether the address changed. Used to know
  // whether a presence-check scan has reconfirmed it yet.
  uint32_t getLastSeenAtMillis() { return lastSeenAtMillis; }
protected:
  std::string name;
  // Set by IotsaBLEClientMod::addDevice() (a friend) at construction time.
  // Lets connect() tell the owning mod when a connect attempt starts/ends,
  // so scanning can be held off while any connection is being established --
  // connections take priority over scanning.
  IotsaBLEClientMod* owner = nullptr;
  BLERemoteCharacteristic *_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID);
  // address/addressValid (and addressType, classic-BLE only) are written
  // from the NimBLE host task (receivedAdvertisement(), via onResult()) and
  // read from both the main loop task and this device's own
  // BLEDimmer::connectionTask() -- protected by addressMutex. Always take it
  // with a short bounded timeout (never portMAX_DELAY) and never call
  // anything blocking (BLE calls, Serial, etc.) while holding it: a real
  // FreeRTOS mutex wait (unlike a portMUX critical section) never disables
  // interrupts, so even a stuck holder can't block the hardware watchdog --
  // worst case is a skipped update this cycle, not a wedged device.
  SemaphoreHandle_t addressMutex;
  BLEAddress address;
#ifdef IOTSA_WITHOUT_NIMBLE
  esp_ble_addr_type_t addressType;
#endif
  bool addressValid;
  uint32_t lastSeenAtMillis = 0;
  BLEClient* pClient;
  const uint8_t connectionTimeoutSeconds = 6; // xxxjack should be configurable
};

#endif // IOTSA_WITH_BLE
#endif
