#ifndef _IOTSABLEBUTTONMOD_H_
#define _IOTSABLEBUTTONMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaInput.h"
#include "iotsaImmediateAlertBLEClient.h"

//
// BLE doorbell button module. When the button is pressed, it rings the
// target, a BLERinger, over BLE. The target's name is set on the web page
// /doorbell or with PUT /api/doorbell {"name": "..."}; nothing
// rings until it is set. The trigger /ring rings too, for curl.
//
class IotsaBLEButtonMod : public IotsaModule {
public:
  IotsaBLEButtonMod(IotsaApplication &_app, IotsaBLEClientMod &bleClientMod, Button &_button)
  : IotsaModule(_app),
    button(_button),
    // The client module finds the target and keeps the connection; the
    // target registers itself there as soon as it has a name.
    ringer(std::string(), "", &bleClientMod)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override {}
  String info() override;
protected:
  void configLoad() override;
  void configSave() override;
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "BLE Doorbell Button"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  void ringTrigger(IotsaWebContext& ctx);
#endif
private:
  void ring();
  String ringStatus();
  Button &button;
  IotsaImmediateAlertBLEClient ringer;
  bool noTargetConfigured = false;
  static const uint32_t ringTimeoutMillis = 10000;
};

#endif
