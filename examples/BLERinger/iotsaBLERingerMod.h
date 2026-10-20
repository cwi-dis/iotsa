#ifndef _IOTSABLERINGERMOD_H_
#define _IOTSABLERINGERMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaImmediateAlertBLEServer.h"

//
// BLE ringer module. A BLEButton (or a BLE tool) rings it over BLE, with the
// Immediate Alert Service; it shows the alert on the status LED.
//
class IotsaBLERingerMod : public IotsaModule {
public:
  IotsaBLERingerMod(IotsaApplication &_app)
  : IotsaModule(_app),
    alertServer([this](uint8_t level) { alert(level); })
  {}
  void setup() override;
  void loop() override {}
  String info() override;
private:
  void alert(uint8_t level);
  IotsaImmediateAlertBLEServer alertServer;
};

#endif
