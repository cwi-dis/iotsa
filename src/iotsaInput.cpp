#include "iotsa.h"
#include "iotsaInput.h"
#include "iotsaConfigFile.h"

#define DEBOUNCE_DELAY 50 // 50 ms debouncing

static void dummyTouchCallback() {}
static bool anyWakeOnTouch;
static uint64_t bitmaskButtonWakeHigh;
static int buttonWakeLow;

void IotsaInputMod::setup() {
  anyWakeOnTouch = false;
  bitmaskButtonWakeHigh = 0;
  buttonWakeLow = -1;
  for(int i=0; i<nInput; i++) {
    inputs[i]->setup();
  }
  esp_err_t err;
  if (bitmaskButtonWakeHigh && buttonWakeLow && anyWakeOnTouch) {
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
  pin(_pin),
  debounceState(false),
  debounceTime(0),
  lastChangeMillis(0),
  firstRepeat(0),
  minRepeat(0),
  curRepeat(0),
  nextRepeat(0)
{
}

void Button::setRepeat(uint32_t _firstRepeat, uint32_t _minRepeat) {
  firstRepeat = _firstRepeat;
  minRepeat = _minRepeat;
}

void Button::setup() {
  pinMode(pin, INPUT_PULLUP);
  if (wake) {
    // Buttons should be wired to GND. So press gives a low level.
    if (actOnPress) {
      if (buttonWakeLow > 0) IotsaSerial.println("Multiple low wake inputs");
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
    // The touchpad has been in the new state for long enough for us to trust it.
    pressed = state;
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
    if (curRepeat > minRepeat) {
      curRepeat = curRepeat - minRepeat;
      if (curRepeat < minRepeat) curRepeat = minRepeat;
    }
    nextRepeat = millis() + curRepeat;
    if (activationCallback) activationCallback();
  }
}


Touchpad::Touchpad(int _pin, bool _actOnPress, bool _actOnRelease, bool _wake)
: Button(_actOnPress, _actOnRelease, _wake),
  threshold(20)
{
}

void Touchpad::setup() {
  if (wake) {
    anyWakeOnTouch = true;
    touchAttachInterrupt(pin, dummyTouchCallback, threshold);
  }
}

bool Touchpad::_getState() {
  uint16_t value = touchRead(pin);
  if (value == 0) return false;
  return value < threshold;
}

RotaryEncoder::RotaryEncoder(int _pinA, int _pinB)
: Input(true, true, false),
  value(0),
  duration(0),
  pinA(_pinA),
  pinB(_pinB),
  pinAstate(false),
  lastChangeMillis(0)
{
}

void RotaryEncoder::setup() {
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinAstate = digitalRead(pinA) == LOW;
  if (wake) {
    // xxxjack unsure about this: would "wake on any high" mean on positive flanks (as I hope) or
    // would this mean the cpu remain awake when any pin is level high? To be determined.
    bitmaskButtonWakeHigh |= 1LL << pinA;
    bitmaskButtonWakeHigh |= 1LL << pinB;
  }

}

void RotaryEncoder::loop() {
  bool pinAnewState = digitalRead(pinA) == LOW;

  if (pinAnewState != pinAstate) {
    if (lastChangeMillis) {
      duration = millis() - lastChangeMillis;
    }
    lastChangeMillis = millis();
    // PinA is in a new state
    pinAstate = pinAnewState;
    // If pinA changed state high read pinB to determine whether this is an increment or a decrement.
    bool pinBstate = digitalRead(pinB) == LOW;
    bool increment = pinAstate != pinBstate;
    if (increment) {
      value++;
    } else {
      value--;
    }
    bool doSend = (increment && actOnPress) || (!increment && actOnRelease);
    IFDEBUG IotsaSerial.printf("RotaryEncoder callback for button pin %d,%d state %d,%d increment %d value %d duration %u\n", pinA, pinB, pinAstate, pinBstate, increment, value, duration);
    if (doSend && activationCallback) {
      activationCallback();
    }
  }
}

UpDownButtons::UpDownButtons(Button& _up, Button& _down)
: Input(true, true, false),
  value(0),
  up(_up),
  down(_down)
{
  up.setCallback(std::bind(&UpDownButtons::_upPressed, this));
  down.setCallback(std::bind(&UpDownButtons::_downPressed, this));
  up.setRepeat(1000, 100);
  down.setRepeat(1000, 100);
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
  value++;
  IFDEBUG IotsaSerial.printf("UpDownButtons callback for button up value %d\n", value);
  if (actOnPress && activationCallback) activationCallback();
  return true;
}

bool UpDownButtons::_downPressed() {
  value--;
  if (actOnRelease && activationCallback) activationCallback();
  return true;
}
