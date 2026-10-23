#ifndef _IOTSAIMMEDIATEALERTBLECLIENT_H_
#define _IOTSAIMMEDIATEALERTBLECLIENT_H_
#include "iotsaRunmodeBLEClient.h"
#include "iotsaImmediateAlertBLE.h"

#ifdef IOTSA_WITH_BLE

// The client side of the Immediate Alert Service: rings a BLERinger. Like
// any IotsaRunmodeBLEClient it can also send the standard runmode commands
// (identify, reboot).
class IotsaImmediateAlertBLEClient : public IotsaRunmodeBLEClient {
public:
  using IotsaRunmodeBLEClient::IotsaRunmodeBLEClient;
  bool setAlertLevel(IotsaImmediateAlertBLE::AlertLevel level);
  bool ring() { return setAlertLevel(IotsaImmediateAlertBLE::HighAlert); }
  // Ring as soon as the device can be reached: the base class connects,
  // calls doWork(), and disconnects. How it went is in getLastWorkStatus();
  // it gives up after deadlineMs.
  void requestRing(uint32_t deadlineMs) { ringPending = true; requestWork(deadlineMs); }
protected:
  bool doWork() override;
  void workAbandoned() override;
  bool ringPending = false;
};

#endif // IOTSA_WITH_BLE
#endif
