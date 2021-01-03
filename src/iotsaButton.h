#ifndef _IOTSABUTTON_H_
#define _IOTSABUTTON_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaRequest.h"
#include <functional>

class Button {
public:
  Button(int _pin, bool _sendOnPress, bool _sendOnRelease) : pin(_pin), sendOnPress(_sendOnPress), sendOnRelease(_sendOnRelease) {}
  int pin;
  bool sendOnPress;
  bool sendOnRelease;
  int debounceState;
  unsigned int debounceTime;
  bool buttonState;
  IotsaRequest req;
};

typedef std::function<void(void)> callback;

#ifdef IOTSA_WITH_API
#define IotsaButtonModBaseMod IotsaApiMod
#else
#define IotsaButtonModBaseMod IotsaMod
#endif

class IotsaButtonMod : IotsaButtonModBaseMod {
public:
  IotsaButtonMod(IotsaApplication &_app, Button* _buttons, int _nButton, IotsaAuthMod *_auth=NULL, callback _successCallback=NULL, callback _failureCallback=NULL)
  : IotsaButtonModBaseMod(_app, _auth),
    buttons(_buttons),
    nButton(_nButton),
    successCallback(_successCallback),
    failureCallback(_failureCallback)
  {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
  void configLoad() override;
  void configSave() override;
  void handler();
  Button* buttons;
  int nButton;
public:
  callback successCallback;
  callback failureCallback;
};

#endif // _IOTSABUTTON_H_