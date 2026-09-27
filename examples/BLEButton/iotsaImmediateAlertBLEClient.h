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
};

#endif // IOTSA_WITH_BLE
#endif
