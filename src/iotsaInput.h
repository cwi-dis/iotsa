#ifndef _IOTSAINPUT_H_
#define _IOTSAINPUT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaRequest.h"

typedef std::function<bool()> ActivationCallbackType;

class Input {
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
  void bindVar(bool& _var, bool _toggle);
  bool pressed;
  uint32_t duration;
  int repeatCount;
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
  bool *boolVar;
  bool toggle;
};

#ifdef ESP32
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
#endif // ESP32

class ValueInput : public Input {
public:
  ValueInput();
  void bindVar(int& _var, int _min, int _max, int _stepSize);
  void bindVar(float& _var, float _min, float _max, float _stepSize);
  int value;
protected:
  void _changeValue(int steps);
  int *intVar, intMin, intMax, intStep;
  float *floatVar, floatMin, floatMax, floatStep;
};

class RotaryEncoder : public ValueInput {
public:
  RotaryEncoder(int _pinA, int _pinB);
  void setup();
  void loop();
  void setAcceleration(uint32_t _accelMillis);
  uint32_t duration;
protected:
  int pinA;
  int pinB;
  bool pinAstate;
  uint32_t lastChangeMillis;
  uint32_t accelMillis;
};

class UpDownButtons : public ValueInput {
public:
  UpDownButtons(Button& _up, Button& _down, bool _useState=false);
  void setup();
  void loop();
  void bindStateVar(bool& _var);
  bool state;
protected:
  Button& up;
  Button& down;
  bool _upPressed();
  bool _downPressed();
  bool useState;
  bool *stateVar;
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
