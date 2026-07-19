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
  // True only once the connection is actually usable (pClient reports
  // CONNECTED). False for every other state, including DISCONNECTING --
  // callers that need to distinguish "fully gone, safe to connect() again"
  // from "still tearing down" should use isDisconnecting() as well.
  bool isConnected();
  // True from the moment disconnect() is called until the disconnect is
  // actually confirmed complete. NimBLEClient::connect() hard-rejects if
  // called while the previous disconnect is still settling, and
  // isConnected() alone can't tell "settling" from "gone" apart (both read
  // as not-connected). Callers should hold off calling connect() while this
  // is true.
  bool isDisconnecting();
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
  // Registered on pClient so we get told when a disconnect actually
  // completes (not just when we ask for one) -- see isDisconnecting().
  class ConnCallbacks : public BLEClientCallbacks {
  public:
    IotsaBLEClientConnection *owner = nullptr;
    void onConnect(BLEClient* pClient) override;
#ifdef IOTSA_WITHOUT_NIMBLE
    void onDisconnect(BLEClient* pClient) override;
#else
    void onDisconnect(BLEClient* pClient, int reason) override;
#endif
  };
  ConnCallbacks connCallbacks;
  // False from the moment disconnect() issues pClient->disconnect() until
  // ConnCallbacks::onDisconnect() confirms it actually completed. connect()
  // waits (bounded) on this before proceeding. volatile: written from the
  // NimBLE host callback context, read from this device's own
  // BLEDimmer::connectionTask().
  volatile bool disconnectSettled = true;
};

#endif // IOTSA_WITH_BLE
#endif
