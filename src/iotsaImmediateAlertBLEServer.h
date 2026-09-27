#ifndef _IOTSAIMMEDIATEALERTBLESERVER_H_
#define _IOTSAIMMEDIATEALERTBLESERVER_H_
#include "iotsaBLEServer.h"
#include "iotsaImmediateAlertBLE.h"

#ifdef IOTSA_WITH_BLE

typedef std::function<void(uint8_t level)> AlertLevelCallback;

// Exposes the Immediate Alert Service (IAS) as a BLE peripheral. Generic --
// takes a callback for "the alert level changed", has no opinion on how the
// device actually alerts (buzzer, LED, ...). See examples/BLERinger for a
// concrete use (a NeoPixel).
class IotsaImmediateAlertBLEServer : public IotsaBLEProvider {
public:
  IotsaImmediateAlertBLEServer(AlertLevelCallback _callback) : callback(_callback) {}
  void setup();
protected:
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
private:
  AlertLevelCallback callback;
  IotsaBleApiService bleApi;
};

#endif // IOTSA_WITH_BLE
#endif
