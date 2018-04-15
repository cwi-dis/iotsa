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
  int debounceTime;
  bool buttonState;
  IotsaRequest req;
};

typedef std::function<void(void)> callback;

class IotsaButtonMod : IotsaApiMod {
public:
  IotsaButtonMod(IotsaApplication &_app, Button* _buttons, int _nButton, IotsaAuthMod *_auth=NULL, callback _successCallback=NULL, callback _failureCallback=NULL)
  : IotsaApiMod(_app, _auth),
    buttons(_buttons),
    nButton(_nButton),
    successCallback(_successCallback),
    failureCallback(_failureCallback)
  {}
  void setup();
  void serverSetup();
  void loop();
  String info();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  Button* buttons;
  int nButton;
public:
  callback successCallback;
  callback failureCallback;
};

#endif // _IOTSABUTTON_H_