#ifndef _IOTSABLINKMOD_H_
#define _IOTSABLINKMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
// Blink module. Blinks an LED on the given pin. The interval (how long the LED
// stays on, and then off) can be changed on the web page /blink or with
// GET/PUT /api/blink. It is not saved: after a reboot it is 1000 ms again.
//
class IotsaBlinkMod : public IotsaModule {
public:
  IotsaBlinkMod(IotsaApplication &_app, int _pin)
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
  String webTitle() override { return "Blink"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  int pin;
  int interval = 1000;        // milliseconds
  bool ledOn = false;
  unsigned long lastToggle = 0;
};

#endif
