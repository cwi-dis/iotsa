#ifndef _IOTSABLEDEVICEINFO_H_
#define _IOTSABLEDEVICEINFO_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

#include <freertos/semphr.h>

// Base class for anything we've seen advertise over BLE, whether or not we
// ever intend to connect to it. Holds only what every such device has in
// common: identity (name/bleAddress) and the most recent advertisement data
// (RSSI, when we last saw it). IotsaBLEClientDevice (connectable, known
// devices) extends this with the heavier connect/disconnect machinery --
// kept separate so devices we're only passively observing (e.g. results of
// an "unknown devices" scan) don't pay for a NimBLEClient* and a mutex they'll
// never use.
class IotsaBLEDeviceInfo {
public:
  IotsaBLEDeviceInfo(std::string _name, std::string _bleAddress="");
  virtual ~IotsaBLEDeviceInfo();
  const std::string& getName() { return bleName; }
  std::string getAddress();
  // Seed a previously-persisted bleAddress, so available()/connect() work
  // before any advertisement has been received (e.g. right after boot).
  void setKnownAddress(const std::string& _bleAddress);
  // Same idea as setKnownAddress(), for the name: lets a subclass that's
  // constructed before its real name is known (e.g. a numbered slot filled
  // in later via config or REST -- see Lissabon::DimmerBLEClient) update it
  // afterwards. Named distinctly (not "setName") because a class that
  // multiply-inherits this alongside something with its own unrelated
  // setName() (e.g. AbstractDimmer's) would otherwise have two, ambiguous.
  void setKnownName(const std::string& _name) { bleName = _name; }
  int getRSSI() { return rssi; }
  // millis() timestamp of the last time we saw an advertisement (or
  // connected to) this device, regardless of whether the bleAddress changed.
  // Used to know whether a presence-check scan has reconfirmed it yet.
  uint32_t getLastSeenAtMillis() { return lastSeenAtMillis; }
  // Records a freshly-seen advertisement: updates rssi and lastSeenAtMillis
  // unconditionally, bleAddress only if it changed. Returns true iff the
  // bleAddress changed (including going from unknown to known).
  virtual bool receivedAdvertisement(const NimBLEAdvertisedDevice& _device);
  // Adds name/bleAddress/rssi/lastSeenMillisAgo to reply (the last two only if
  // we've ever actually seen an advertisement). Subclasses extend this
  // (calling it first) to add their own fields.
  virtual bool getHandler(JsonObject& reply);
protected:
  // Never portMAX_DELAY: bounds how long any caller (including loop()) can
  // possibly wait on bleAddressMutex, so a stuck holder degrades to a skipped
  // update, not a wedged task. The lock is only ever held for a plain field
  // copy, so contention this long should never actually happen.
  static constexpr TickType_t bleAddressMutexTimeout = pdMS_TO_TICKS(20);
  std::string bleName;
  // bleAddress/bleAddressValid are written from the NimBLE host task
  // (receivedAdvertisement()) and read from other tasks (e.g. a per-device
  // connection task in the IotsaBLEClientDevice subclass) -- protected
  // by bleAddressMutex. Always take it with a short bounded timeout (never
  // portMAX_DELAY) and never call anything blocking (BLE calls, Serial,
  // etc.) while holding it: a real FreeRTOS mutex wait (unlike a portMUX
  // critical section) never disables interrupts, so even a stuck holder
  // can't block the hardware watchdog -- worst case is a skipped update this
  // cycle, not a wedged device.
  SemaphoreHandle_t bleAddressMutex;
  NimBLEAddress bleAddress;
  bool bleAddressValid;
  int rssi = 0;
  uint32_t lastSeenAtMillis = 0;
};

#endif // IOTSA_WITH_BLE
#endif
