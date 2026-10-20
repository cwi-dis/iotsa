#ifndef _IOTSADIMMERMOD_H_
#define _IOTSADIMMERMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaInput.h"

//
// Dimmer module. A button switches it on and off, a rotary encoder sets the
// level, 0 to 100. Read-only for the outside world: a web page /dimmer and
// GET /api/dimmer show what the controls did. It doesn't drive a lamp; a
// real dimmer would.
//
class IotsaDimmerMod : public IotsaModule {
public:
  IotsaDimmerMod(IotsaApplication &_app, Button &_button, RotaryEncoder &_encoder)
  : IotsaModule(_app),
    button(_button),
    encoder(_encoder)
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
  Button &button;
  RotaryEncoder &encoder;
  bool on = false;
  int level = 50;
};

#endif
