#ifndef _IOTSAALARMMOD_H_
#define _IOTSAALARMMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
// Alarm module. Sounds a buzzer on the given pin for a while: on the web page
// /alarm, with PUT /api/alarm, or with the trigger /ring?alarm=20, a plain URL
// for curl or another device (the Button example). Durations are in tenths
// of a second.
//
class IotsaAlarmMod : public IotsaModule {
public:
  IotsaAlarmMod(IotsaApplication &_app, int _pin)
  : IotsaModule(_app),
    pin(_pin)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Alarm Server"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  void ringTrigger(IotsaWebContext& ctx);
#endif
private:
  void _ring(int dur);
  int pin;
  unsigned long alarmStart = 0;
  unsigned long alarmDuration = 0;  // milliseconds, 0 when not ringing
};

#endif
