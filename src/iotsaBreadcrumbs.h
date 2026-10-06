#ifndef _IOTSABREADCRUMBS_H_
#define _IOTSABREADCRUMBS_H_

#include <stdint.h>

// Intended to be included from iotsa.h

//
// Breadcrumbs (cwi-dis/iotsa#276): a small record in RTC memory that survives
// every reset except a power cycle, so that after a watchdog reset, a crash or
// a reboot you can still see what the device was doing when it died.
//
// Two parts:
// - The activity word: "I am now doing X". Overwritten, cheap enough (one
//   32-bit write) for hot paths. iotsa sets it before every module's loop(),
//   around API requests and BLE callbacks, during OTA and before sleeping.
// - The event ring: 30 rare, meaningful events, appended. At boot the previous
//   activity word is moved into the ring, followed by the boot cause.
//
// Both use the same 32-bit entry: 8-bit code, 8-bit arg, 16-bit uptime in
// units of 65.536 s (millis() >> 16, wraps with millis() after ~49.7 days).
// Codes 0-127 are iotsa's (below), 128-255 are the application's.
//
// iotsaBreadcrumbsEnabled = false stops all writes after boot (the boot record
// itself is always written), to measure what breadcrumbs cost.
//

enum IotsaBreadcrumbCode : uint8_t {
  IOTSA_CRUMB_NONE = 0,
  // Activities
  IOTSA_CRUMB_BOOTING = 1,        // setup()/lateSetup()
  IOTSA_CRUMB_LOOP = 2,           // arg: module index, early modules first
  IOTSA_CRUMB_CORE = 3,           // not in any module: iotsaController, the sketch's loop(), the Arduino core
  IOTSA_CRUMB_REQUEST = 4,        // arg: IotsaBreadcrumbTransport
  IOTSA_CRUMB_OTA = 5,            // an OTA transfer is running
  IOTSA_CRUMB_BLE_CALLBACK = 6,   // arg: 1 read, 2 write (NimBLE host task)
  IOTSA_CRUMB_SLEEP = 7,          // going to sleep
  // Events
  IOTSA_CRUMB_BOOT = 64,          // arg: platform reset reason
  IOTSA_CRUMB_IOTSA_WATCHDOG = 65,// iotsa's own watchdog fired (the platform reports a software reboot)
  IOTSA_CRUMB_REBOOT = 66,        // a software reboot; arg: 0 requested, 1 after OTA
  IOTSA_CRUMB_FACTORY_RESET = 67,
  // Application codes start here
  IOTSA_CRUMB_APP = 128
};

enum IotsaBreadcrumbTransport : uint8_t {
  IOTSA_CRUMB_REST = 1,
  IOTSA_CRUMB_COAP = 2,
  IOTSA_CRUMB_HPS = 3
};

extern bool iotsaBreadcrumbsEnabled;

class IotsaBreadcrumbs {
public:
  static constexpr int RING_SIZE = 30;

  void begin();                                    // once, first thing in IotsaApplication::setup()
  void setActivity(uint8_t code, uint8_t arg = 0);
  void addBreadcrumb(uint8_t code, uint8_t arg = 0);
  uint32_t activity();                             // raw activity word, for IotsaActivityScope
  void restoreActivity(uint32_t word);             // code and arg of word, current uptime
  bool bootedByIotsaWatchdog() { return _iotsaWatchdog; }
  // Copy the ring, oldest first, into entries (RING_SIZE long). Returns the count.
  int events(uint32_t *entries);
  static uint8_t entryCode(uint32_t entry) { return entry >> 24; }
  static uint8_t entryArg(uint32_t entry) { return (entry >> 16) & 0xff; }
  static uint32_t entryUptime(uint32_t entry) { return (uint64_t)(entry & 0xffff) * 65536 / 1000; }  // seconds
  static const char *codeName(uint8_t code);       // nullptr for unknown and application codes
private:
  void _append(uint32_t entry);
  bool _iotsaWatchdog = false;
};

// Called from iotsa's watchdog interrupt, just before it restarts the device.
void iotsaBreadcrumbsMarkIotsaWatchdog();

extern IotsaBreadcrumbs iotsaBreadcrumbs;

// Sets an activity for the duration of a scope, then puts the previous one back,
// so a short callback or request doesn't hide what loop() was doing. With two
// tasks (loop and NimBLE) on two cores the restored word can be stale; on a
// single core the NimBLE task preempts loop(), and it is exact.
class IotsaActivityScope {
public:
  IotsaActivityScope(uint8_t code, uint8_t arg = 0) : _previous(iotsaBreadcrumbs.activity()) {
    iotsaBreadcrumbs.setActivity(code, arg);
  }
  ~IotsaActivityScope() { iotsaBreadcrumbs.restoreActivity(_previous); }
  IotsaActivityScope(const IotsaActivityScope&) = delete;
  IotsaActivityScope& operator=(const IotsaActivityScope&) = delete;
private:
  uint32_t _previous;
};

#endif
