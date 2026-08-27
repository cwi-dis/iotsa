#ifndef _IOTSAALARMMOD_H_
#define _IOTSAALARMMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

#define PIN_ALARM 4 // GPIO4 connects to the buzzer

//
// Buzzer module. Has a web interface and a REST interface (and/or COAP
// interface, based on iotsa configuration options).
//
class IotsaAlarmMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  using IotsaBaseModule::needsAuthentication;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
private:
  unsigned long alarmEndTime = 0;
};

#endif
