#ifndef _IOTSAINPUT_H_
#define _IOTSAINPUT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaRequest.h"

typedef std::function<bool()> ActivationCallbackType;

class Input : public IotsaRequestContainer {
public:
  Input(bool _actOnPress, bool _actOnRelease, bool _wake=false);
  void setCallback(ActivationCallbackType callback);
  virtual void setup() = 0;
  virtual void loop() = 0;
protected:
  bool actOnPress;
  bool actOnRelease;
  bool wake;
  ActivationCallbackType activationCallback;
};

class Button : public Input {
public:
  Button(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake=false);
  void setup();
  void loop();
  void setRepeat(uint32_t _firstRepeat, uint32_t _minRepeat);
  bool pressed;
  uint32_t duration;
protected:
  virtual bool _getState();
  int pin;
  bool debounceState;
  unsigned int debounceTime;
  uint32_t lastChangeMillis;
  uint32_t firstRepeat;
  uint32_t minRepeat;
  uint32_t curRepeat;
  uint32_t nextRepeat;
};

class Touchpad : public Button {
public:
  Touchpad(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake=false);
  void setup();
  // void loop() is used from Button
  bool pressed;
  uint32_t duration;
protected:
  bool _getState();
  uint16_t threshold;
};

class RotaryEncoder : public Input {
public:
  RotaryEncoder(int _pinA, int _pinB);
  void setup();
  void loop();
  int value;
  uint32_t duration;
protected:
  int pinA;
  int pinB;
  bool pinAstate;
  uint32_t lastChangeMillis;
};

class UpDownButtons : public Input {
public:
  UpDownButtons(Button& _up, Button& _down);
  void setup();
  void loop();
  int value;
protected:
  Button& up;
  Button& down;
  bool _upPressed();
  bool _downPressed();
};

class IotsaInputMod : public IotsaMod {
public:
  IotsaInputMod(IotsaApplication& app, Input **_inputs, int _nInput) : IotsaMod(app), inputs(_inputs), nInput(_nInput) {}
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info() { return ""; }
protected:
  Input **inputs;
  int nInput;

};

#endif
