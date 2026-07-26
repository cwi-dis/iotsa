#ifndef _IOTSABLEDEVICEINFO_H_
#define _IOTSABLEDEVICEINFO_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBle.h"

#ifdef IOTSA_WITH_BLE

#include <freertos/semphr.h>

// Base class for anything we've seen advertise over BLE, whether or not we
// ever intend to connect to it. Holds only what every such device has in
// common: identity (name/address) and the most recent advertisement data
// (RSSI, when we last saw it). IotsaBLEClientConnection (connectable, known
// devices) extends this with the heavier connect/disconnect machinery --
// kept separate so devices we're only passively observing (e.g. results of
// an "unknown devices" scan) don't pay for a BLEClient* and a mutex they'll
// never use.
class IotsaBLEDeviceInfo {
public:
  IotsaBLEDeviceInfo(std::string _name, std::string _address="");
  virtual ~IotsaBLEDeviceInfo();
  const std::string& getName() { return name; }
  std::string getAddress();
  // Seed a previously-persisted address, so available()/connect() work
  // before any advertisement has been received (e.g. right after boot).
  void setKnownAddress(const std::string& _address);
  int getRSSI() { return rssi; }
  // millis() timestamp of the last time we saw an advertisement (or
  // connected to) this device, regardless of whether the address changed.
  // Used to know whether a presence-check scan has reconfirmed it yet.
  uint32_t getLastSeenAtMillis() { return lastSeenAtMillis; }
  // Records a freshly-seen advertisement: updates rssi and lastSeenAtMillis
  // unconditionally, address only if it changed. Returns true iff the
  // address changed (including going from unknown to known).
  virtual bool receivedAdvertisement(const BLEAdvertisedDevice& _device);
  // Adds name/address/rssi/lastSeenMillisAgo to reply (the last two only if
  // we've ever actually seen an advertisement). Subclasses extend this
  // (calling it first) to add their own fields.
  virtual void getHandler(JsonObject& reply);
protected:
  // Never portMAX_DELAY: bounds how long any caller (including loop()) can
  // possibly wait on addressMutex, so a stuck holder degrades to a skipped
  // update, not a wedged task. The lock is only ever held for a plain field
  // copy, so contention this long should never actually happen.
  static constexpr TickType_t addressMutexTimeout = pdMS_TO_TICKS(20);
  std::string name;
  // address/addressValid (and addressType, classic-BLE only) are written
  // from the NimBLE host task (receivedAdvertisement()) and read from other
  // tasks (e.g. a per-device connection task in the IotsaBLEClientConnection
  // subclass) -- protected by addressMutex. Always take it with a short
  // bounded timeout (never portMAX_DELAY) and never call anything blocking
  // (BLE calls, Serial, etc.) while holding it: a real FreeRTOS mutex wait
  // (unlike a portMUX critical section) never disables interrupts, so even a
  // stuck holder can't block the hardware watchdog -- worst case is a
  // skipped update this cycle, not a wedged device.
  SemaphoreHandle_t addressMutex;
  BLEAddress address;
#ifdef IOTSA_WITHOUT_NIMBLE
  esp_ble_addr_type_t addressType;
#endif
  bool addressValid;
  int rssi = 0;
  uint32_t lastSeenAtMillis = 0;
};

#endif // IOTSA_WITH_BLE
#endif
