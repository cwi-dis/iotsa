#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBle.h"
#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE

#include <set>
#include <map>

typedef std::function<void(const BLEAdvertisedDevice&)> BleDeviceFoundCallback;
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
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig);

  virtual void loop() override;
  virtual String info() override { return ""; }
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
  void deviceNotConnectable(std::string id);
  void deviceNotConnectable(String id) { deviceNotConnectable(std::string(id.c_str())); }
  // Seed a known device's persisted address, so it can be found/connected
  // to directly without waiting for (or requiring) a matching advertisement.
  void noteKnownAddress(std::string id, std::string address);
  void noteKnownAddress(String id, String address) { noteKnownAddress(std::string(id.c_str()), std::string(address.c_str())); }
  bool canConnect();
  unsigned int maxConnectionKeepOpen();
  //
  // Interfaces to control which BLE devices are visible to this
  // class (and any subclass)
  //
  void findUnknownDevices(bool on);
  void setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback);
  void setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback);
  void setDuplicateNameFilter(bool noDuplicates);
  void setServiceFilter(const BLEUUID& serviceUUID);
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
  // These are all names of unknown devices
  std::set<std::string> unknownDevices;
protected:
  void configLoad();
  void configSave();
  void onResult(const BLEAdvertisedDevice *advertisedDevice);
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
  void setupScanner();
  void updateScanning();
  void startScanning();
  void stopScanning();
  virtual void scanningChanged() {}
  bool isScanning();
  void startScanUnknown();
  static IotsaBLEClientMod *scanningMod;
  int scan_interval = 155;
  int scan_window = 151;
  bool scanForUnknownClients = false;
  uint32_t scanUnknownUntilMillis = 0;
  uint32_t shouldUpdateScanAtMillis = 0;
  const int noScanMillis = 4000;
  BLEScan *scanner = NULL;
  BleDeviceFoundCallback unknownDeviceCallback = NULL;
  BleDeviceFoundCallback knownDeviceCallback = NULL;
  bool duplicateNameFilter = false;
  BLEUUID* serviceFilter = NULL;
  uint16_t manufacturerFilter;
  bool hasManufacturerFilter = false;
  bool advertisingWasPausedByScan = false;
};

#endif // IOTSA_WITH_BLE
#endif
