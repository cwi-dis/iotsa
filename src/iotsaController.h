#ifndef _IOTSACONTROLLER_H_
#define _IOTSACONTROLLER_H_
#include <stdint.h>

// Intended to be included from iotsa.h

//
// IotsaController -- the device policy coordinator (cwi-dis/iotsa#106).
//
// A framework global (like iotsaConfig / iotsaStatus), ticked from
// IotsaApplication::loop(). It will own the interlocking device-level policies
// -- the iotsa_mode state machine, radio-enablement, sleep/wake, reboot -- that
// are currently scattered across IotsaConfig and IotsaBatteryMod, and will
// drive IotsaWifiController and the BLE server as subordinates. See
// docs/controller-architecture.md.
//
// Being built up incrementally. So far it owns only the deferred-reboot timer,
// moved here from IotsaConfig::loop().
//

class IotsaController {
public:
  void tick();                       // from IotsaApplication::loop()
  void requestReboot(uint32_t ms);   // ESP.restart() after ms milliseconds

private:
  uint32_t _rebootAtMillis = 0;
};

extern IotsaController iotsaController;
#endif
