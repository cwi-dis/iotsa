#ifndef _IOTSADIMMERMOD_H_
#define _IOTSADIMMERMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaInput.h"

//
// Dimmer module. A button switches it on and off, a rotary encoder sets the
// level, 0 to 100. It dims an LED on the given pin. Read-only for the
// outside world: a web page /dimmer and GET /api/dimmer show what the
// controls did.
//
class IotsaDimmerMod : public IotsaModule {
public:
  IotsaDimmerMod(IotsaApplication &_app, Button &_button, RotaryEncoder &_encoder, int _pin, bool _activeLow = false)
  : IotsaModule(_app),
    button(_button),
    encoder(_encoder),
    pin(_pin),
    activeLow(_activeLow)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override {}
  String info() override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Dimmer"; }
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  void _update();
  Button &button;
  RotaryEncoder &encoder;
  int pin;
  bool activeLow;  // the LED lights when the pin is LOW
  bool on = false;
  int level = 50;
};

#endif
