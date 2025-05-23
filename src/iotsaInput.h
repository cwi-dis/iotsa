#ifndef _IOTSAINPUT_H_
#define _IOTSAINPUT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaRequest.h"

#if ESP32
#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0) 
// Jack _thinks_ that soc_caps.h was introduced in version 4.
//
// With the various variants of the esp32 SoCs we need more fine-grained tests than just ESP32
#include "soc/soc_caps.h"
// Check whether this SoC supports external wakeup from deep sleep
#if SOC_PM_SUPPORT_EXT_WAKEUP
#define IOTSA_WITH_WAKEUP_SUPPORT 1
#endif
// Check whether this SoC has touch sensors
#if SOC_TOUCH_SENSOR_NUM > 0
#define IOTSA_WITH_TOUCH_SUPPORT 1
#endif
// Check whether this variant supports the ESP32Encoder library (esp32c3 does not)
#if SOC_PCNT_SUPPORTED
#define IOTSA_WITH_ESP32ENCODER_LIB 1
#endif
#else
// If we don't have soc_caps.h we need a different way to determine this.
// xxxjack to be determined.
#endif // ESP_IDF_VERSION
#endif // ESP32

//#define IOTSA_DEBUG_INPUT

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
  void setup() override;
  void loop() override;
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

#if IOTSA_WITH_TOUCH_SUPPORT
class Touchpad : public Button {
public:
  Touchpad(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake=false);
  void setup() override;
  // void loop() is used from Button
  bool pressed;
  uint32_t duration;
#ifdef IOTSA_DEBUG_INPUT
  uint16_t dbg_lastValue;
#endif
protected:
  virtual bool _getState() override;
#ifdef IOTSA_DEBUG_INPUT
public:
#endif
  uint16_t threshold;
};
#endif // IOTSA_WITH_TOUCH_SUPPORT

class ValueInput : public Input {
public:
  ValueInput();
  void bindVar(int& _var, int _min, int _max, int _stepSize);
  void bindVar(float& _var, float _min, float _max, float _stepSize);
  int value;
protected:
  bool _changeValue(int steps);
  int *intVar, intMin, intMax, intStep;
  float *floatVar, floatMin, floatMax, floatStep;
};


class ESP32Encoder;
class RotaryEncoder : public ValueInput {
public:
  RotaryEncoder(int _pinA, int _pinB);
  void setup() override;
  void loop() override;
  void setAcceleration(uint32_t _accelMillis);
  uint32_t duration;
protected:
#ifdef IOTSA_WITH_ESP32ENCODER_LIB
  ESP32Encoder *_encoder;
  int64_t oldCount = 0;
#else
  int pinA;
  int pinB;
  bool pinAstate;
#endif
  uint32_t lastChangeMillis;
  uint32_t accelMillis;
};

class UpDownButtons : public ValueInput {
public:
  UpDownButtons(Button& _up, Button& _down, bool _useState=false);
  void setup() override;
  void loop() override;
  void bindStateVar(bool& _var);
  void setStateCallback(ActivationCallbackType callback);
  bool state;
protected:
  Button& up;
  Button& down;
  bool _upPressedCallback();
  bool _downPressedCallback();
  bool useState;
  bool *stateVar;
  ActivationCallbackType stateCallback;
};

class CyclingButton : public ValueInput {
public:
  CyclingButton(Button& _button);
  void setup() override;
  void loop() override;
  void bindStateVar(bool& _var);
  void setStateCallback(ActivationCallbackType callback);
  bool state;
protected:
  Button& button;
  bool _pressedCallback();
  bool *stateVar;
  int direction = 1;
  ActivationCallbackType stateCallback;
};

class IotsaInputMod : public IotsaMod {
public:
  IotsaInputMod(IotsaApplication& app, Input **_inputs, int _nInput) : IotsaMod(app), inputs(_inputs), nInput(_nInput) {}
  using IotsaMod::IotsaMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override { return ""; }
#endif
protected:
  Input **inputs;
  int nInput;

};

#endif
