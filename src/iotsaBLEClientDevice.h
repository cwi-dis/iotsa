#ifndef _IOTSABLECLIENTDEVICE_H_
#define _IOTSABLECLIENTDEVICE_H_
#include "iotsa.h"
#include "iotsaBLE.h"
#include "iotsaBLEDeviceInfo.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_BLE

#include <freertos/semphr.h>

typedef std::function<void(uint8_t *, size_t)> BleNotificationCallback;

class IotsaBLEClientMod;

// Also implements IotsaApiModObject (cwi-dis/iotsa#268): every subclass
// (Lissabon::DimmerBLEClient, IotsaImmediateAlertBLEClient, ...) gets a
// complete "named/addressed/persistable/reportable BLE device" -- config
// persistence, REST reporting/renaming, web-form rendering -- for free,
// instead of each one reimplementing the identity half by hand. A subclass
// that also inherits some *other* IotsaApiModObject (e.g. DimmerBLEClient
// also extends AbstractDimmer) will hit the same same-signature-on-two-
// unrelated-bases situation as available()/isConnected()/getHandler()
// below, and needs the same explicit-qualification treatment.
class IotsaBLEClientDevice : public IotsaBLEDeviceInfo, public IotsaApiModObject {
  friend class IotsaBLEClientMod;
public:
  // _owner: optional, lets a caller-constructed device (one that isn't
  // handed to IotsaBLEClientMod::addDevice() until later, e.g.
  // Lissabon::DimmerBLEClient or examples/BLEButton's IotsaImmediateAlertBLEClient)
  // register itself later via retarget() without the mod having to reach
  // back in and set the (otherwise friend-only) owner field itself.
  IotsaBLEClientDevice(const std::string& _name, std::string _bleAddress="", IotsaBLEClientMod* _owner=nullptr);
  ~IotsaBLEClientDevice();
  bool receivedAdvertisement(const NimBLEAdvertisedDevice& _device) override;
  void clearDevice();
  bool available();
  // Requests any in-progress scan to stop (connecting and scanning are
  // mutually exclusive on this stack) and reports whether a connect attempt
  // is worth making right now. False means "not yet, try again soon" -- not
  // "gave up"; callers should keep whatever they were about to send/request
  // pending rather than treat this the same as a failed connect() call.
  // Generic home for a dance every caller previously had to reimplement by
  // hand via a direct reference to the owning IotsaBLEClientMod -- only
  // BLEDimmer ever did (and only on its IOTSA_WITH_BLE_TASKS path; the
  // non-tasks loop() path never requested the scan stop at all, so it could
  // get stuck behind a full discovery scan) (cwi-dis/iotsa#143).
  bool canConnect();
  bool connect();
  void disconnect();
  // Disconnects (if needed) and returns the NimBLEClient slot to the shared
  // pool, unlike disconnect() alone which keeps holding it. Call this once a
  // connection has gone idle; connect() will lazily create a fresh client
  // next time it's actually needed. Also called from the destructor.
  void release();
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
  bool getAsNotification(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, BleNotificationCallback callback);
  // Adds connect-specific fields (on top of the base class's
  // name/bleAddress/rssi/lastSeenMillisAgo) to reply: lastConnectAttemptMillisAgo,
  // numConnectCalls, numConnectSkipped, numConnectAttempts, numConnectFailed,
  // numConnectSucceeded, numConnectionOpen, numConnectionFailed,
  // numConnectionClosedLocally, lastDisconnectReason, lastDisconnectMillisAgo.
  void getHandler(JsonObject& reply) override;
  // f_name is the usual per-instance config-key/form-field prefix (e.g.
  // "dimmer3"), matching every other IotsaModObject implementation's
  // convention (see e.g. IotsaRequest). Persists/reports/renders name +
  // resolved address; connect stats above are report-only (getHandler),
  // never persisted or web-editable.
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
  // Renaming via REST: {"name": "<newName>"}. Nothing else about this
  // object is meaningfully settable from outside (address is discovered,
  // not configured).
  bool putHandler(const JsonVariant& request) override;
#ifdef IOTSA_WITH_WEB
  // Shows a found/connected status line always; the editable name field
  // only when includeConfig (renaming isn't a day-to-day control).
  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  void formHandler_TD(String& message, bool includeConfig) override;
  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
#endif
  // Renames this device: updates the persisted identity and, if already
  // registered with an owning IotsaBLEClientMod (owner != nullptr), re-keys
  // that registration too (delDevice old name + addDevice new name) so
  // future advertisements/discovery match the new target. Returns false
  // (a no-op) if newName is unchanged. Does NOT perform the *first*
  // registration -- a device that has never been added to any mod at all
  // (owner still nullptr, e.g. one just default-constructed with an empty
  // name and no _owner argument either) still needs that done explicitly by
  // its caller.
  bool retarget(const std::string& newName);
  // Drives whatever outbound work this device has queued for itself (if
  // any) -- connects when reachable, fires it, disconnects. Default: no-op,
  // this base class has no notion of queued work. IotsaRunmodeBLEClient
  // overrides this to service a pending identify/reboot/promoteMode/
  // setWifiDisabled request (cwi-dis/iotsa#264's BLEController needed a way
  // to actually control a device, not just discover/name it).
  // IotsaBLEClientCollectionMod::loop() calls this on every known device
  // each tick; a single-target consumer (e.g. examples/BLEButton) that
  // drives its own device's connection directly has no need to call it.
  virtual void serviceIfNeeded() {}
protected:
  // Set at construction time (the optional _owner constructor argument) or
  // by IotsaBLEClientMod::addDevice() (a friend), whichever happens first.
  // Lets connect() tell the owning mod when a connect attempt starts/ends,
  // so scanning can be held off while any connection is being established --
  // connections take priority over scanning. Also lets retarget() re-key
  // this device's own registration on a rename.
  IotsaBLEClientMod* owner = nullptr;
  NimBLERemoteCharacteristic *_getCharacteristic(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID);
  NimBLEClient* pClient = nullptr;
  // Registered on pClient so we get told when a disconnect actually
  // completes (not just when we ask for one) -- see isDisconnecting().
  class ConnCallbacks : public NimBLEClientCallbacks {
  public:
    IotsaBLEClientDevice *owner = nullptr;
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
  // Counted from the point connect() knows it has a valid bleAddress (i.e. is
  // actually going to skip or attempt) -- connect() called without an
  // bleAddress at all, or a mutex-timeout bailout, are both exceptional paths
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
  // Deliberately separate from bleAddressValid/available(): a failed connect
  // doesn't mean the bleAddress is wrong (e.g. a lightSleep device just happened
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
