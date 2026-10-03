#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLE.h"
#include "iotsaBLEDeviceInfo.h"
#include "iotsaBLEClientDevice.h"

#ifdef IOTSA_WITH_BLE

#include <set>
#include <map>
#include <atomic>

typedef std::function<void(const NimBLEAdvertisedDevice&)> BleDeviceFoundCallback;
typedef const char *UUIDString;

// Bare scanner + single-outgoing-connect arbiter (cwi-dis/iotsa#263) + named-
// device registry -- the minimum any BLE-central consumer needs, whether it
// tracks one device (examples/BLEButton) or many. Deliberately does NOT
// include a human-facing "browse/manage known and unknown devices" REST/web
// surface, or unknown-device discovery -- that's IotsaBLEClientCollectionMod
// (iotsaBLEClientCollection.h), which subclasses this. Split out
// cwi-dis/iotsa#264: found while asking why examples/BLEButton -- which only
// ever needs a reference to its single ringer -- had to carry a whole
// "collection" class at all.
class IotsaBLEClientMod : public IotsaModule, public NimBLEScanCallbacks {
public:
  using IotsaModule::IotsaModule;
  virtual ~IotsaBLEClientMod() {}
  virtual bool getHandler(const char *path, JsonObject& reply) override;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  virtual void setup() override;
  virtual void lateSetup() override;
  virtual void loop() override;
  //
  // Interfaces for use by subclasses (or other classes with a reference)
  // to control which BLE devices are known by this class
  //
  // Registers `id` as a known device. If `device` is null (the common
  // case), this mod constructs a plain IotsaRunmodeBLEClient itself -- fine
  // for any caller that doesn't need its own subclass. If `device` is given,
  // it's already fully constructed by the caller (e.g. Lissabon::DimmerBLEClient
  // passing itself, via its IotsaRunmodeBLEClient base) and this mod just
  // starts tracking it -- necessary whenever the caller's own constructor
  // needs more than just an id (DimmerBLEClient's does), or needs the id
  // known before this mod could construct anything (this mod's constructor
  // path requires the id upfront; DimmerBLEClient gets its name later, via
  // setName()).
  IotsaBLEClientDevice* addDevice(std::string id, IotsaBLEClientDevice* device = nullptr);
  IotsaBLEClientDevice* addDevice(String id, IotsaBLEClientDevice* device = nullptr) { return addDevice(std::string(id.c_str()), device); }
  IotsaBLEClientDevice* getDevice(std::string id);
  IotsaBLEClientDevice* getDevice(String id) { return getDevice(std::string(id.c_str())); }
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
  // Claim/release the device-wide outgoing-connect slot: thin wrappers around
  // IotsaBLERadioArbiter::tryBeginConnect()/endActivity() (cwi-dis/iotsa#263),
  // adding this mod's connectSettleTimeMillis. Called by
  // IotsaBLEClientDevice::connect() right before pClient->connect(). Unlike
  // canConnect() (a cheap non-atomic peek) this is the real, race-free gate.
  // releaseConnectSlot() must be called exactly once for every acquire that
  // returned true, whatever the connect outcome.
  bool tryAcquireConnectSlot();
  void releaseConnectSlot();
  // Called by IotsaBLEClientDevice::connect() (via its owner back-
  // pointer) when a connect attempt fails and sets needsRescan. Pokes the
  // scan scheduler so loop() re-evaluates needsDiscovery() promptly, instead
  // of waiting for some unrelated event (a different device's advertisement,
  // a user-requested scan, ...) to happen to touch shouldUpdateScanAtMillis
  // -- otherwise a failed connect's needsRescan flag can go unnoticed
  // indefinitely, since nothing else schedules another look.
  void requestScanUpdate();
  unsigned int maxConnectionKeepOpen();
  // True if another client link may be opened now: fewer than
  // maxOpenClientConnections devices are connecting or lingering. Consulted by
  // IotsaBLEClientDevice::service() before it starts a connect; when it says
  // no, loop() closes a lingering link early (cwi-dis/iotsa#263 decision 2).
  // Only counts devices driven by the state machine -- not links opened
  // through the blocking IotsaBLEClientDevice::connect() (lissabon, until
  // it moves over).
  bool mayOpenLink();
  // Read by IotsaBLEClientDevice::connect() via its owner back-pointer.
  uint32_t getConnectTimeoutMillis() { return connectTimeoutMillis; }
  void setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback);
  //
  // If true, scanning pauses advertising for the duration of the scan, via
  // IotsaBLERadioArbiter's PAUSE_SCAN reason. Off by default: this only ever
  // affects apps that use both a client and a server together.
  //
  static bool coordinateWithServer;
protected:
  // These are all the known devices (known by the application, not by this module)
  std::map<std::string, IotsaBLEClientDevice*> devices;
  // These are all known devices by address
  std::map<std::string, IotsaBLEClientDevice *>devicesByAddress;
protected:
  // Named distinctly from "configLoad"/"configSave" (not just "virtual"), on
  // purpose: a subclass meant to be used as an app's own module (e.g.
  // LissabonControllerMod, LissabonRemoteMod) already has its own, unrelated
  // IotsaModule-level configLoad()/configSave() for its own app config --
  // same name, same signature, would otherwise silently become an
  // unintended override of *this* class's scan-tuning persistence the
  // moment either became virtual (found while adding
  // IotsaBLEClientCollectionMod's own config field, cwi-dis/iotsa#264).
  virtual void loadScanConfig();
  virtual void saveScanConfig();
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice);
  // Called from onResult() for an advertisement that doesn't match any known
  // device (by name or address) -- i.e. a candidate this class itself has no
  // opinion on. Default: ignore it entirely. IotsaBLEClientCollectionMod
  // overrides this to populate its unknownDevices listing (subject to its
  // own isInterestingUnknownDevice() filter).
  virtual void onUnknownDeviceSeen(const NimBLEAdvertisedDevice* advertisedDevice, const std::string& deviceName) {}
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
  void setupScanner();
  void updateScanning();
  void startScanning();
  void stopScanning();
  virtual void scanningChanged() {}
  bool isScanning();
  // True if we still need to actively look for devices: some known device
  // has no address yet (never matched by name), or a known device just
  // failed a connect attempt and needs reconfirming (see
  // IotsaBLEClientDevice::needsRescan). False means there is currently no
  // reason to scan at all. IotsaBLEClientCollectionMod overrides this to
  // also return true while hunting for unknown devices.
  virtual bool needsDiscovery();
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
  // How long a single IotsaBLEClientDevice::connect() call waits for the
  // link to establish before giving up (NimBLEClient::setConnectTimeout()).
  // Read via getConnectTimeoutMillis() by IotsaBLEClientDevice through
  // its owner back-pointer, since the timeout is only actually applied once,
  // when a device's pClient is first created.
  uint32_t connectTimeoutMillis = 6000;
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
  // How many client links the state machine keeps open at once (connecting +
  // lingering). 1: a device that needs the radio makes a lingering one close
  // early (cwi-dis/iotsa#263 decision 2). A constant per app, not tunable.
  int maxOpenClientConnections = 1;
  uint32_t scanStartedAtMillis = 0;
  uint32_t shouldUpdateScanAtMillis = 0;
  // Only loop() (and the functions it calls: startScanning/stopScanning) may
  // write this. Cross-task scan/connect exclusion no longer reads it (that's
  // IotsaBLERadioArbiter's activity now); volatile kept for isScanning().
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
  BleDeviceFoundCallback knownDeviceCallback = NULL;
};

#endif // IOTSA_WITH_BLE
#endif
