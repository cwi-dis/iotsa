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
  bool receivedAdvertisement(const NimBLEAdvertisedDevice& _device) override;
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
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const uint8_t *data, size_t size);
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t value);
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t value);
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t value);
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const std::string& value);
  bool set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const String& value);
  bool get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t& value);
  bool get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t& value);
  bool get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t& value);
  bool get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, std::string& value);
  bool getAsBuffer(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t **datap, size_t *sizep);
  bool getAsNotification(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, BleNotificationCallback callback);
  // Adds connect-specific fields (on top of the base class's
  // name/address/rssi/lastSeenMillisAgo) to reply: lastConnectAttemptMillisAgo,
  // numConnectCalls, numConnectSkipped, numConnectAttempts, numConnectFailed,
  // numConnectSucceeded, numConnectionOpen, numConnectionFailed,
  // numConnectionClosedLocally, lastDisconnectReason, lastDisconnectMillisAgo.
  void getHandler(JsonObject& reply) override;
protected:
  // Set by IotsaBLEClientMod::addDevice() (a friend) at construction time.
  // Lets connect() tell the owning mod when a connect attempt starts/ends,
  // so scanning can be held off while any connection is being established --
  // connections take priority over scanning.
  IotsaBLEClientMod* owner = nullptr;
  NimBLERemoteCharacteristic *_getCharacteristic(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID);
  NimBLEClient* pClient = nullptr;
  // Registered on pClient so we get told when a disconnect actually
  // completes (not just when we ask for one) -- see isDisconnecting().
  class ConnCallbacks : public NimBLEClientCallbacks {
  public:
    IotsaBLEClientConnection *owner = nullptr;
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;
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
  // this only moves on an actual new pClient->connect() call. Records the
  // attempt itself, not whether it succeeded -- see numConnectFailed/
  // numConnectSucceeded for the outcome.
  uint32_t lastConnectAttemptAtMillis = 0;
  // connect()'s own call-count bookkeeping, fully closed:
  //   numConnectCalls = numConnectSkipped + numConnectAttempts
  //   numConnectAttempts = numConnectFailed + numConnectSucceeded
  // Counted from the point connect() knows it has a valid address (i.e. is
  // actually going to skip or attempt) -- connect() called without an
  // address at all, or a mutex-timeout bailout, are both exceptional paths
  // no current caller exercises (BLEDimmer always checks available() first)
  // and are deliberately left out of this tree rather than diluting it.
  uint32_t numConnectCalls = 0;
  // connect() found pClient already connected -- a lingering connection was
  // reused, no new link had to be established.
  uint32_t numConnectSkipped = 0;
  uint32_t numConnectAttempts = 0;
  uint32_t numConnectFailed = 0;
  uint32_t numConnectSucceeded = 0;
  // Separate tree, tracking the eventual fate of connections that DID
  // succeed (numConnectSucceeded), independent of how many numConnectSkipped
  // reuse-hits happened while any one of them was alive. Also fully closed:
  //   numConnectSucceeded = numConnectionOpen (0 or 1, isConnected() at
  //     report time, not separately stored) + numConnectionFailed +
  //     numConnectionClosedLocally
  // numConnectionFailed is incremented in onDisconnect() when the disconnect
  // wasn't one we asked for (see ConnCallbacks::onDisconnect()) -- e.g.
  // reason=520 Connection Timeout, a lightSleep peer's supervision timeout
  // lapsing while still connected. numConnectionClosedLocally is the
  // complement: disconnect() was called on our own initiative (e.g. the
  // keepopen idle timer) and the disconnect completed as expected.
  uint32_t numConnectionFailed = 0;
  uint32_t numConnectionClosedLocally = 0;
  // True once a connect attempt has failed, until reachability is
  // reconfirmed (a matching advertisement, or a successful connect).
  // Deliberately separate from addressValid/available(): a failed connect
  // doesn't mean the address is wrong (e.g. a lightSleep device just happened
  // to be asleep), so it must not force a costly rediscovery-by-name scan.
  // Consulted by IotsaBLEClientMod::needsDiscovery() to trigger a rescan.
  bool needsRescan = false;
  // NimBLE host-stack reason code from the most recent onDisconnect(), or -1
  // if none seen yet. Distinguishes "connect succeeded, then something went
  // wrong later" from a plain failed connect attempt (which never reaches
  // onConnect()/onDisconnect() at all). Decoded via
  // NimBLEUtils::returnCodeToString() for REST/debug output.
  int lastDisconnectReason = -1;
  // millis() timestamp of the most recent onDisconnect(), 0 if none seen yet.
  // Set alongside lastDisconnectReason, regardless of whether that disconnect
  // counted as numConnectionFailed or numConnectionClosedLocally.
  uint32_t lastDisconnectAtMillis = 0;
};

#endif // IOTSA_WITH_BLE
#endif
