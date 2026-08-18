#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBle.h"
#include "iotsaBLEDeviceInfo.h"
#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE

#include <set>
#include <map>
#include <atomic>

typedef std::function<void(const NimBLEAdvertisedDevice&)> BleDeviceFoundCallback;
typedef const char *UUIDString;

class IotsaBLEClientMod : public IotsaApiMod, public NimBLEScanCallbacks {
public:
  using IotsaApiMod::IotsaApiMod;
  virtual ~IotsaBLEClientMod() {}
  virtual bool getHandler(const char *path, JsonObject& reply) override;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  virtual String formHandler_field_perdevice(const char *deviceName);
  virtual void setup() override;
  virtual void serverSetup() override;
  virtual void handler();
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig);
#ifdef IOTSA_WITH_WEB
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig);
#endif

  virtual void loop() override;
#ifdef IOTSA_WITH_WEB
  virtual String info() override { return ""; }
#endif
  //
  // Interfaces for use by subclasses (or other classes with a reference)
  // to control which BLE devices are known by this class
  //
  IotsaBLEClientConnection* addDevice(std::string id);
  IotsaBLEClientConnection* addDevice(String id) { return addDevice(std::string(id.c_str())); }
  IotsaBLEClientConnection* getDevice(std::string id);
  IotsaBLEClientConnection* getDevice(String id) { return getDevice(std::string(id.c_str())); }
  void delDevice(std::string id);
  void delDevice(String id) { delDevice(std::string(id.c_str())); }
  // Seed a known device's persisted address, so it can be found/connected
  // to directly without waiting for (or requiring) a matching advertisement.
  void noteKnownAddress(std::string id, std::string address);
  void noteKnownAddress(String id, String address) { noteKnownAddress(std::string(id.c_str()), std::string(address.c_str())); }
  bool canConnect();
  // If a scan is currently running, request that it be stopped right away so
  // a caller that wants to connect doesn't have to wait for it to finish on
  // its own -- connecting and scanning are mutually exclusive on this stack
  // (NimBLE rejects connect attempts outright while a scan is active). Safe
  // to call from ANY task (e.g. a per-device BLEDimmer::connectionTask()) --
  // this only sets a flag; the actual stopScanning() call is deferred to
  // loop(), which is the only task allowed to touch scanner/scanningMod.
  void requestStopScanningForConnect();
  // Called by IotsaBLEClientConnection::connect() (via its owner back-
  // pointer) around the actual pClient->connect() call, from whichever task
  // owns that connection (e.g. BLEDimmer::connectionTask()). Connections
  // take priority over scanning: updateScanning() refuses to start a new
  // scan while connectingCount > 0. std::atomic, so plain increment/decrement
  // from any task is safe without extra locking.
  void noteConnectAttemptStarted();
  void noteConnectAttemptEnded();
  // Called by IotsaBLEClientConnection::connect() (via its owner back-
  // pointer) when a connect attempt fails and sets needsRescan. Pokes the
  // scan scheduler so loop() re-evaluates needsDiscovery() promptly, instead
  // of waiting for some unrelated event (a different device's advertisement,
  // a user-requested scan, ...) to happen to touch shouldUpdateScanAtMillis
  // -- otherwise a failed connect's needsRescan flag can go unnoticed
  // indefinitely, since nothing else schedules another look.
  void requestScanUpdate();
  unsigned int maxConnectionKeepOpen();
  // Read by IotsaBLEClientConnection::connect() via its owner back-pointer.
  uint32_t getConnectTimeoutMillis() { return connectTimeoutMillis; }
  //
  // Interfaces to control which BLE devices are visible to this
  // class (and any subclass)
  //
  void findUnknownDevices(bool on);
  void setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback);
  void setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback);
  void setDuplicateNameFilter(bool noDuplicates);
  void setServiceFilter(const NimBLEUUID& serviceUUID);
  void setManufacturerFilter(uint16_t manufacturerID);
  //
  // If true, scanning pauses IotsaBLEServerMod's advertising for the duration
  // of the scan (and resumes it afterwards). Off by default: this only ever
  // affects apps that use both a client and a server together.
  //
  static bool coordinateWithServer;
protected:
  // These are all the known devices (known by the application, not by this module)
  std::map<std::string, IotsaBLEClientConnection*> devices;
  // These are all known devices by address
  std::map<std::string, IotsaBLEClientConnection *>devicesByAddress;
  // Devices seen advertising that aren't in `devices` above -- keyed by
  // name. Lighter-weight than IotsaBLEClientConnection (no NimBLEClient*, no
  // connect machinery) since most of these are only ever seen in passing.
  std::map<std::string, IotsaBLEDeviceInfo*> unknownDevices;
protected:
  void configLoad();
  void configSave();
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice);
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
  void setupScanner();
  void updateScanning();
  void startScanning();
  void stopScanning();
  virtual void scanningChanged() {}
  bool isScanning();
  void startScanUnknown();
  // True if we still need to actively look for devices: either explicitly
  // hunting for unknown devices, some known device has no address yet
  // (never matched by name), or a known device just failed a connect
  // attempt and needs reconfirming (see IotsaBLEClientConnection::needsRescan).
  // False means there is currently no reason to scan at all.
  bool needsDiscovery();
  static IotsaBLEClientMod *scanningMod;
  int scan_interval = 155;
  int scan_window = 151;
  // All durations/cooldowns below are deliberately configurable (REST +
  // persisted config, like scan_interval/scan_window): the right values
  // depend on the interplay with server-side advertise duration and
  // sleep/wake cycle timing, which varies per deployment. Millis suffix
  // matches the codebase-wide convention for millisecond-unit fields
  // (stayConnectedMillis, animationDurationMillis, postponeSleepMillis, etc.).
  uint32_t scanDurationDiscoveryMillis = 11000;  // scan length while actively looking for unknown/unaddressed devices
  uint32_t scanCooldownDiscoveryMillis = 4000;   // minimum gap before starting another discovery scan
  uint32_t connectSettleTimeMillis = 100;        // grace period after scanning stops before a connect() is attempted
  // How long a single IotsaBLEClientConnection::connect() call waits for the
  // link to establish before giving up (NimBLEClient::setConnectTimeout()).
  // Read via getConnectTimeoutMillis() by IotsaBLEClientConnection through
  // its owner back-pointer, since the timeout is only actually applied once,
  // when a device's pClient is first created.
  uint32_t connectTimeoutMillis = 6000;
  // How long the manual "scan for unknown devices" session (REST scanUnknown
  // flag, or the web form's "Scan for Nms" button) stays active before
  // findUnknownDevices(false) turns it back off. This is a session length,
  // not a single scan's duration -- during the session, updateScanning()
  // still runs its normal discovery-scan/cooldown cycle
  // (scanDurationDiscoveryMillis/scanCooldownDiscoveryMillis) repeatedly.
  // Currently independent of those two; may eventually be derived from them
  // instead (e.g. N full discovery cycles) but for now it's its own
  // configurable value, kept at its original default.
  uint32_t scanUnknownDurationMillis = 20000;
  // maxConnectionKeepOpen()'s fallback when there's no scheduled scan
  // deadline to respect -- how long a connection may be held open with
  // nothing else pending. shouldUpdateScanAtMillis is 0 not just when idle,
  // but also while a scan is actively running (nothing reschedules it until
  // the scan stops), so this binds more often than "idle" alone would
  // suggest. Not REST/persisted-configurable like the fields above: this is
  // an application characteristic (how long a connection legitimately needs
  // to stay open -- lissabon's quick-follow-up-command use case is very
  // different from e.g. a continuous sensor-polling or audio-transfer
  // application), meant to be overridden by a subclass's constructor, not
  // tuned per-deployment by an end user.
  uint32_t noScheduledScanKeepOpenCapMillis = 30000;
  uint32_t scanStartedAtMillis = 0;
  // Written by loop()/stopScanning(), read from other tasks by canConnect()
  // (e.g. BLEDimmer::connectionTask()) -- volatile so those reads see fresh
  // values across tasks.
  volatile uint32_t scanStoppedAtMillis = 0;
  bool scanForUnknownClients = false;
  uint32_t scanUnknownUntilMillis = 0;
  uint32_t shouldUpdateScanAtMillis = 0;
  // Only loop() (and the functions it calls: startScanning/stopScanning) may
  // write this. canConnect(), called from other tasks, only reads it -- hence
  // volatile, same reasoning as scanStoppedAtMillis above.
  NimBLEScan * volatile scanner = NULL;
  // Set from onScanEnd(), which NimBLE calls on its own host task. Only this
  // flag is touched from that context; the actual stopScanning() call (which
  // mutates scanner/scanningMod) must stay on the single main loop() task, or
  // it races with loop()'s own access to the same state.
  volatile bool scanHasEnded = false;
  // Set from requestStopScanningForConnect(), which may be called from any
  // task (e.g. a per-device BLEDimmer::connectionTask()). Same deferral
  // pattern as scanHasEnded -- only loop() acts on it.
  volatile bool scanStopRequested = false;
  // Number of IotsaBLEClientConnection::connect() calls currently blocked
  // inside pClient->connect(), across all devices/tasks. updateScanning()
  // refuses to start a scan while this is > 0 -- connections take priority.
  std::atomic<int> connectingCount{0};
  BleDeviceFoundCallback unknownDeviceCallback = NULL;
  BleDeviceFoundCallback knownDeviceCallback = NULL;
  bool duplicateNameFilter = false;
  NimBLEUUID* serviceFilter = NULL;
  uint16_t manufacturerFilter;
  bool hasManufacturerFilter = false;
  bool advertisingWasPausedByScan = false;
};

#endif // IOTSA_WITH_BLE
#endif
