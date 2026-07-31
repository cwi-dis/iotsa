#ifndef _IOTSABLECLIENTCONNECTION_H_
#define _IOTSABLECLIENTCONNECTION_H_
#include "iotsa.h"
#include "iotsaBle.h"
#include "iotsaBLEDeviceInfo.h"

#ifdef IOTSA_WITH_BLE

#include <freertos/semphr.h>

typedef std::function<void(uint8_t *, size_t)> BleNotificationCallback;

class IotsaBLEClientMod;

class IotsaBLEClientConnection : public IotsaBLEDeviceInfo {
  friend class IotsaBLEClientMod;
public:
  IotsaBLEClientConnection(std::string& _name, std::string _address="");
  ~IotsaBLEClientConnection();
  bool receivedAdvertisement(const BLEAdvertisedDevice& _device) override;
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
  // Adds connect-specific fields (on top of the base class's
  // name/address/rssi/lastSeenMillisAgo) to reply: lastConnectMillisAgo,
  // numSuccessfulConnections, numFailedConnectionAttempts,
  // lastDisconnectReason.
  void getHandler(JsonObject& reply) override;
protected:
  // Set by IotsaBLEClientMod::addDevice() (a friend) at construction time.
  // Lets connect() tell the owning mod when a connect attempt starts/ends,
  // so scanning can be held off while any connection is being established --
  // connections take priority over scanning.
  IotsaBLEClientMod* owner = nullptr;
  BLERemoteCharacteristic *_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID);
  BLEClient* pClient = nullptr;
  // Registered on pClient so we get told when a disconnect actually
  // completes (not just when we ask for one) -- see isDisconnecting().
  class ConnCallbacks : public BLEClientCallbacks {
  public:
    IotsaBLEClientConnection *owner = nullptr;
    void onConnect(BLEClient* pClient) override;
    void onDisconnect(BLEClient* pClient, int reason) override;
  };
  ConnCallbacks connCallbacks;
  // False from the moment disconnect() issues pClient->disconnect() until
  // ConnCallbacks::onDisconnect() confirms it actually completed. connect()
  // waits (bounded) on this before proceeding. volatile: written from the
  // NimBLE host callback context, read from this device's own
  // BLEDimmer::connectionTask().
  volatile bool disconnectSettled = true;
  // millis() timestamp of the start of the most recent genuine connect
  // attempt -- i.e. NOT the connect()-while-already-connected fast path, so
  // this only moves on an actual new pClient->connect() call.
  uint32_t lastConnectAtMillis = 0;
  uint32_t numSuccessfulConnections = 0;
  uint32_t numFailedConnectionAttempts = 0;
  // NimBLE host-stack reason code from the most recent onDisconnect(), or -1
  // if none seen yet. Distinguishes "connect succeeded, then something went
  // wrong later" from a plain failed connect attempt (which never reaches
  // onConnect()/onDisconnect() at all). Decoded via
  // NimBLEUtils::returnCodeToString() for REST/debug output.
  int lastDisconnectReason = -1;
};

#endif // IOTSA_WITH_BLE
#endif
