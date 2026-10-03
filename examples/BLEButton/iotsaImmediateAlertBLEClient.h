#ifndef _IOTSAIMMEDIATEALERTBLECLIENT_H_
#define _IOTSAIMMEDIATEALERTBLECLIENT_H_
#include "iotsaRunmodeBLEClient.h"
#include "iotsaImmediateAlertBLE.h"

#ifdef IOTSA_WITH_BLE

// Client-side counterpart to IotsaImmediateAlertBLEServer. Inherits the
// generic runmode commands (identify/reboot/etc.) for free, same as any
// other IotsaRunmodeBLEClient subclass (e.g. Lissabon::DimmerBLEClient) --
// a second, non-dimmer protocol validating that this pattern generalizes.
class IotsaImmediateAlertBLEClient : public IotsaRunmodeBLEClient {
public:
  using IotsaRunmodeBLEClient::IotsaRunmodeBLEClient;
  bool setAlertLevel(IotsaImmediateAlertBLE::AlertLevel level);
  bool ring() { return setAlertLevel(IotsaImmediateAlertBLE::HighAlert); }
  // Ring as soon as the device can be reached: the base class's connection
  // state machine connects, calls doWork(), and disconnects (cwi-dis/iotsa#263).
  // Outcome in getLastWorkStatus(); gives up after deadlineMs.
  void requestRing(uint32_t deadlineMs) { ringPending = true; requestWork(deadlineMs); }
protected:
  bool doWork() override;
  void workAbandoned() override;
  bool ringPending = false;
};

#endif // IOTSA_WITH_BLE
#endif
