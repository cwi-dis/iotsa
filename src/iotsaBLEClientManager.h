#ifndef _IOTSABLECLIENTMANAGER_H_
#define _IOTSABLECLIENTMANAGER_H_
#include "iotsaBLEClient.h"
#include "iotsaBLEClientCollection.h"

#ifdef IOTSA_WITH_BLE

// Adds the "human-facing set of known BLE devices" layer on top of
// IotsaBLEClientMod's bare scanner+arbiter+registry (cwi-dis/iotsa#264):
// REST/web listing of known devices (reusing each device's own
// IotsaApiModObject surface, cwi-dis/iotsa#268), add/remove/rename by name,
// and scanning for/listing unknown/unclaimed devices nearby.
//
// Was IotsaBLEClientCollectionMod: it manages devices, it isn't a collection
// of them. The devices added through it are owned and saved by its
// IotsaBLEClientCollection (cwi-dis/iotsa#286), in /config/bleclient.cfg
// next to the scan settings.
//
// Subclass THIS, not IotsaBLEClientMod directly, if you want a human-facing
// "here are the BLE devices I know about, and here's what else is nearby"
// page/API (e.g. examples/BLEManager). A consumer that only ever needs a
// reference to one or a few specific, already-named devices (e.g.
// examples/BLEButton, Lissabon::DimmerBLEClient's owner) should keep using
// the bare IotsaBLEClientMod -- it never needed this layer, just the
// scanner/arbiter/registry underneath it.
class IotsaBLEClientManagerMod : public IotsaBLEClientMod {
public:
  using IotsaBLEClientMod::IotsaBLEClientMod;
  virtual bool getHandler(const char *path, JsonObject& reply) override;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  virtual void loop() override;
#ifdef IOTSA_WITH_WEB
  virtual String formHandler_field_perdevice(const char *deviceName);
  // text: what the devices are called, plural, used in the headings ("Known
  // <text>", "Unknown <text>"). webHandler() passes devicesLabel().
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig);
  virtual const char *devicesLabel() { return "BLE devices"; }
  virtual void webHandler() override;
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig);
  virtual String info() override {
    return "<p>See <a href='/bleclient'>/bleclient</a> for known/unknown BLE devices, or <a href='/api/bleclient'>/api/bleclient</a> for the REST equivalent.</p>";
  }
#endif
  //
  // Interfaces to control which unknown BLE devices are surfaced by this
  // class (and any subclass)
  //
  void findUnknownDevices(bool on);
  void setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback);
  //
  // What REST/web "add"/"remove" (by name) actually do. Return true if
  // something changed. Default: add to or remove from `collection`, and
  // save. To get devices of your own type, give `collection` a factory
  // (setFactory()) rather than overriding these. A subclass that keeps its
  // devices elsewhere (lissabonController, until cwi-dis/lissabon#37) or has
  // a fixed set (lissabonRemote, which returns false) overrides them.
  virtual bool addDeviceByName(const std::string& name);
  virtual bool removeDeviceByName(const std::string& name);
protected:
  void clientConfigLoad(IotsaConfigFileLoad& cf) override;
  void clientConfigSave(IotsaConfigFileSave& cf) override;
  // The devices added through "add", see addDeviceByName().
  IotsaBLEClientCollection collection{*this};
  virtual bool needsDiscovery() override;
  void onUnknownDeviceSeen(const NimBLEAdvertisedDevice* advertisedDevice, const std::string& deviceName) override;
  void startScanUnknown();
  // Gates entry into unknownDevices (and unknownDeviceCallback) -- i.e. "is
  // this advertisement worth treating as a candidate for this collection at
  // all." Does NOT affect already-known devices (addDevice()'d by name,
  // tracked by the IotsaBLEClientMod base), which always match regardless.
  // Default: everything is interesting; override to filter (e.g. by service
  // UUID -- see examples/BLEManager, which only wants other iotsa
  // devices).
  virtual bool isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) { return true; }
  // Devices seen advertising that aren't in the base class's `devices` --
  // keyed by name. Lighter-weight than IotsaBLEClientDevice (no
  // NimBLEClient*, no connect machinery) since most of these are only ever
  // seen in passing.
  std::map<std::string, IotsaBLEDeviceInfo*> unknownDevices;
  BleDeviceFoundCallback unknownDeviceCallback = NULL;
  bool scanForUnknownClients = false;
  uint32_t scanUnknownUntilMillis = 0;
  // How long the manual "scan for unknown devices" session (REST scanUnknown
  // flag, or the web form's "Scan for Nms" button) stays active before
  // findUnknownDevices(false) turns it back off. This is a session length,
  // not a single scan's duration -- during the session, the base class's
  // normal discovery-scan/cooldown cycle still runs repeatedly.
  uint32_t scanUnknownDurationMillis = 20000;
};

#endif // IOTSA_WITH_BLE
#endif
