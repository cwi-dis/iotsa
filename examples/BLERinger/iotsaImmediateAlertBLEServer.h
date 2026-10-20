#ifndef _IOTSAIMMEDIATEALERTBLESERVER_H_
#define _IOTSAIMMEDIATEALERTBLESERVER_H_
#include "iotsaBLEServer.h"
#include "iotsaImmediateAlertBLE.h"

#ifdef IOTSA_WITH_BLE

typedef std::function<void(uint8_t level)> AlertLevelCallback;

// The server side of the Immediate Alert Service: a BLE service with one
// characteristic, the alert level, that a client writes. What alerting means
// is up to the callback.
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
