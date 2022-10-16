#include "iotsa.h"
#include "iotsaInput.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_ESP32ENCODER_LIB
#include <ESP32Encoder.h>
#endif

#define DEBOUNCE_DELAY 50 // 50 ms debouncing

#ifdef IOTSA_WITH_TOUCH_SUPPORT
static void dummyTouchCallback() {}
#endif // IOTSA_WITH_TOUCH_SUPPORT

static bool anyWakeOnTouch;
static uint64_t bitmaskButtonWakeHigh;
static int buttonWakeLow = -1;

void IotsaInputMod::setup() {
  anyWakeOnTouch = false;
  bitmaskButtonWakeHigh = 0;
  buttonWakeLow = -1;
  for(int i=0; i<nInput; i++) {
    inputs[i]->setup();
  }
#if IOTSA_WITH_WAKEUP_SUPPORT
  esp_err_t err;
  if (bitmaskButtonWakeHigh && buttonWakeLow >= 0 && anyWakeOnTouch) {
    IotsaSerial.println("IotsaInputMod: too many incompatible wakeup sources");
  }
  if (anyWakeOnTouch) {
    IFDEBUG IotsaSerial.println("IotsaInputMod: enable wake on touch");
    err = esp_sleep_enable_touchpad_wakeup();
    if (err != ESP_OK) IotsaSerial.println("Error in touchpad_wakeup");
  }
  if (bitmaskButtonWakeHigh) {
    IFDEBUG IotsaSerial.println("IotsaInputMod: enable wake on some high pins");
    err = esp_sleep_enable_ext1_wakeup(bitmaskButtonWakeHigh, ESP_EXT1_WAKEUP_ANY_HIGH);
    if (err != ESP_OK) IotsaSerial.println("Error in ext1_wakeup HIGH");
  }
  if (buttonWakeLow >= 0) {
    if (!anyWakeOnTouch) {
      IFDEBUG IotsaSerial.println("IotsaInputMod: enable wake on one low pins");
      err = esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonWakeLow, 0);
      if (err != ESP_OK) IotsaSerial.println("Error in ext0_wakeup");
    } else {
      err = esp_sleep_enable_ext1_wakeup(1<<buttonWakeLow, ESP_EXT1_WAKEUP_ALL_LOW);
      if (err != ESP_OK) IotsaSerial.println("Error in ext1_wakeup LOW");
    }
  }
#else
  if (anyWakeOnTouch || buttonWakeLow >= 0 || bitmaskButtonWakeHigh) {
    IotsaSerial.println("Wake-from-sleep not implemented on esp8266 or esp32c3");
  }
#endif
}

void IotsaInputMod::serverSetup() {
}

void IotsaInputMod::loop() {

  for (int i=0; i<nInput; i++) {
    inputs[i]->loop();
  }
}

Input::Input(bool _actOnPress, bool _actOnRelease, bool _wake)
: actOnPress(_actOnPress), 
  actOnRelease(_actOnRelease), 
  wake(_wake), 
  activationCallback(NULL)
{
}

void Input::setCallback(ActivationCallbackType callback)
{
  activationCallback = callback;
}

Button::Button(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake)
: Input(_actOnPress, _actOnRelease, _wake),
  pressed(false),
  duration(0),
  repeatCount(0),
  pin(_pin),
  debounceState(false),
  debounceTime(0),
  lastChangeMillis(0),
  firstRepeat(0),
  minRepeat(0),
  curRepeat(0),
  nextRepeat(0),
  boolVar(NULL),
  toggle(false)
{
}

void Button::setRepeat(uint32_t _firstRepeat, uint32_t _minRepeat) {
  firstRepeat = _firstRepeat;
  minRepeat = _minRepeat;
}

void Button::bindVar(bool& _var, bool _toggle) {
  boolVar = &_var;
  if (!_toggle) *boolVar = pressed;
  toggle = _toggle;
}

void Button::setup() {
  pinMode(pin, INPUT_PULLUP);
  if (wake) {
    // Buttons should be wired to GND. So press gives a low level.
    if (actOnPress) {
      if (buttonWakeLow >= 0) IotsaSerial.println("Multiple low wake inputs");
      buttonWakeLow = pin;
    } else {
      bitmaskButtonWakeHigh |= 1LL << pin;
    }
  }

}

bool Button::_getState() {
  return digitalRead(pin) == LOW;
}

void Button::loop() {
  bool state = _getState();
  if (state != debounceState) {
    // The touchpad seems to have changed state. But we want
    // it to remain in the new state for some time (to cater for 50Hz/60Hz interference)
    debounceTime = millis();
    iotsaConfig.postponeSleep(DEBOUNCE_DELAY*2);
  }
  debounceState = state;
  if (millis() > debounceTime + DEBOUNCE_DELAY && state != pressed) {
    iotsaConfig.postponeSleep(0);
    // The touchpad or button has been in the new state for long enough for us to trust it.
    pressed = state;
    if (pressed) repeatCount = 0;
    if (boolVar) {
      if (toggle) {
        if (pressed) {
          *boolVar = !*boolVar;
        }
      } else {
        *boolVar = pressed;
      }
    }
    if (lastChangeMillis) {
      duration = millis() - lastChangeMillis;
    }
    lastChangeMillis = millis();
    if (pressed) {
      // Setup for repeat, if wanted
      if (firstRepeat) {
        curRepeat = firstRepeat;
        nextRepeat = millis() + curRepeat;
      } else {
        nextRepeat = 0;
      }
    } else {
      // Cancel any repeating
      nextRepeat = 0;
    }
    bool doSend = (pressed && actOnPress) || (!pressed && actOnRelease);
    IFDEBUG IotsaSerial.printf("Button callback for button pin %d state %d\n", pin, state);
    if (doSend && activationCallback) {
      activationCallback();
    }
  }
  // See if we need to do any repeating
  if (nextRepeat && millis() > nextRepeat) {
    iotsaConfig.postponeSleep(0);
    if (curRepeat > minRepeat) {
      curRepeat = curRepeat - minRepeat;
      if (curRepeat < minRepeat) curRepeat = minRepeat;
    }
    nextRepeat = millis() + curRepeat;
    repeatCount++;
    if (activationCallback) activationCallback();
  }
}

#if IOTSA_WITH_TOUCH_SUPPORT
Touchpad::Touchpad(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake)
: Button(_pin, _actOnPress, _actOnRelease, _wake),
#ifdef IOTSA_DEBUG_INPUT
  dbg_lastValue(0),
#endif
  threshold(20)
{
  // Initialize threshold by taking some readings, and assuming two thirds of the minimum reading is a good threshold.
  uint16_t minRead = touchRead(pin);
  for (int i=0; i< 10; i++) {
    uint16_t newRead = touchRead(pin);
    if (newRead < minRead) minRead = newRead;
  }
  if (minRead > 20) {
    threshold = minRead*2/3;
  }
}

void Touchpad::setup() {
  IFDEBUG IotsaSerial.printf("touch(%d): threshold=%d\n", pin, threshold);
  if (wake) {
    anyWakeOnTouch = true;
    touchAttachInterrupt(pin, dummyTouchCallback, threshold);
  }
}

bool Touchpad::_getState() {
  uint16_t value = touchRead(pin);
#ifdef IOTSA_DEBUG_INPUT
  dbg_lastValue = value;
#endif
  if (value == 0) return false;
  return value < threshold;
}
#endif // IOTSA_WITH_TOUCH_SUPPORT

ValueInput::ValueInput()
: Input(true, true, false),
  value(0),
  intVar(NULL),
  intMin(0),
  intMax(0),
  intStep(0),
  floatVar(NULL),
  floatMin(0),
  floatMax(0),
  floatStep(0)
{
}

void ValueInput::bindVar(int& _var, int _min, int _max, int _stepSize) {
  intVar = &_var;
  intMin = _min;
  intMax = _max;
  intStep = _stepSize;
}

void ValueInput::bindVar(float& _var, float _min, float _max, float _stepSize) {
  floatVar = &_var;
  floatMin = _min;
  floatMax = _max;
  floatStep = _stepSize;
}


void ValueInput::_changeValue(int steps) {
  value += steps;
  IFDEBUG IotsaSerial.printf("ValueInput callback increment %d value %d", steps, value);
  if (intVar) {
    *intVar += steps*intStep;
    if (*intVar < intMin) *intVar = intMin;
    if (*intVar > intMax) *intVar = intMax;
    IFDEBUG IotsaSerial.printf(" intVar %d", *intVar);
  }
  if (floatVar) {
    *floatVar += steps*floatStep;
    if (*floatVar < floatMin) *floatVar = floatMin;
    if (*floatVar > floatMax) *floatVar = floatMax;
    IFDEBUG IotsaSerial.printf(" floatVar %f", *floatVar);
  }
  IFDEBUG IotsaSerial.println();
  if (activationCallback) {
    activationCallback();
  }
}

RotaryEncoder::RotaryEncoder(int _pinA, int _pinB)
: ValueInput(),
  duration(0),
#ifdef IOTSA_WITH_ESP32ENCODER_LIB
  _encoder(new ESP32Encoder()),
#endif
  lastChangeMillis(0),
  accelMillis(0)
{
#ifdef IOTSA_WITH_ESP32ENCODER_LIB
  ESP32Encoder::useInternalWeakPullResistors=UP;
  // Sigh... It seems we (or ESP32Encoder?) had reversed the pins... Or the edges...
  _encoder->attachHalfQuad(_pinB, _pinA);
  _encoder->clearCount();
#endif
}

void RotaryEncoder::setAcceleration(uint32_t _accelMillis) {
  accelMillis = _accelMillis;
}

void RotaryEncoder::setup() {
#ifndef IOTSA_WITH_ESP32ENCODER_LIB
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinAstate = digitalRead(pinA) == LOW;
  if (wake) {
    // xxxjack unsure about this: would "wake on any high" mean on positive flanks (as I hope) or
    // would this mean the cpu remain awake when any pin is level high? To be determined.
    bitmaskButtonWakeHigh |= 1LL << pinA;
    bitmaskButtonWakeHigh |= 1LL << pinB;
  }
#endif
}

void RotaryEncoder::loop() {
#ifdef IOTSA_WITH_ESP32ENCODER_LIB
  int64_t newCount = _encoder->getCount();
  if (newCount != oldCount) {
    iotsaConfig.postponeSleep(0);
    IotsaSerial.printf("RotaryEncoder %lld->%lld\n", oldCount, newCount);
    int delta = (newCount-oldCount);
    oldCount = newCount;
    _changeValue(delta);
  }
#else
  // Poll state of the pins manually. Note that this can easily miss
  // transitions if there are modules with loop() methods that don't
  // return quickly.
  bool pinAnewState = digitalRead(pinA) == LOW;

  if (pinAnewState != pinAstate) {
    iotsaConfig.postponeSleep(0);
    if (lastChangeMillis) {
      duration = millis() - lastChangeMillis;
    }
    lastChangeMillis = millis();
    // PinA is in a new state
    pinAstate = pinAnewState;
    // If pinA changed state high read pinB to determine whether this is an increment or a decrement.
    bool pinBstate = digitalRead(pinB) == LOW;
    bool increment = pinAstate != pinBstate;
    int change = 1;
    if (accelMillis > 0 && duration > 0) {
      // Check if we want to do multiple steps, because the encoder was 
      // rotated fast
      if (duration < accelMillis) {
        change += accelMillis / duration;
      }
    }
    IotsaSerial.printf("RotaryEncoder pinA=%d pinB=%d increment=%d change=%d\n", pinAstate, pinBstate, increment, change);
    if (increment) {
      _changeValue(change);
    } else {
      _changeValue(-change);
    }
  }
#endif
}

UpDownButtons::UpDownButtons(Button& _up, Button& _down, bool _useState)
: ValueInput(),
  state(false),
  up(_up),
  down(_down),
  useState(_useState),
  stateVar(NULL)
{
  up.setCallback(std::bind(&UpDownButtons::_upPressed, this));
  down.setCallback(std::bind(&UpDownButtons::_downPressed, this));
  up.setRepeat(500, 100);
  down.setRepeat(500, 100);
}

void UpDownButtons::bindStateVar(bool& _var) {
  stateVar = &_var;
}

void UpDownButtons::setStateCallback(ActivationCallbackType callback) {
  stateCallback = callback;
}

void UpDownButtons::setup() {
  up.setup();
  down.setup();
}

void UpDownButtons::loop() {
  up.loop();
  down.loop();
}

bool UpDownButtons::_upPressed() {
  if (!up.pressed) return true;
  if (useState) {
    // The buttons double as on/off buttons. A short press means "on"
    // only longer press (repeats) means "increase".
    if (up.repeatCount == 0) {
      state = true;
      if (stateVar) *stateVar = true;
      if (stateCallback) stateCallback();
      return true;
    }
  }
  _changeValue(1);
  return true;
}

bool UpDownButtons::_downPressed() {
  if (useState) {
    // The buttons double as on/off buttons. A short press means "off"
    // only longer press (repeats) means "decrease". We determine
    // what to do at the release of the down button.
    // We ignore the first press.
    if (down.pressed && down.repeatCount == 0) return true;
    if (down.repeatCount == 0 && !down.pressed) {
      // This was a release that had no repeats. Treat it as off.
      state = false;
      if (stateVar) *stateVar = false;
      if (stateCallback) stateCallback();
      return true;
    }
  }
  if (!down.pressed) return true;
  _changeValue(-1);
  return true;
}
