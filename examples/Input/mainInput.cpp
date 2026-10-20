//
// A "dimmer" controlled by hand: a pushbutton switches it on and off, a
// rotary encoder sets the level. It shows the Input module (iotsaInput.h),
// which reads buttons, rotary encoders and touchpads and changes your
// variables for you. The result can be seen on the web page /dimmer and
// with GET /api/dimmer. See iotsaDimmerMod.cpp.
//
// The button and the encoder connect to ground; the pins use their internal
// pull-ups. For an Alps EC12D encoder with pushbutton, seen from the top:
// northwest (A) and northeast (B) are the encoder pins, southwest (C) their
// ground, north (E) and southeast (D) the pushbutton. Connect C and E to
// ground, A to ENCODER_PIN_A, B to ENCODER_PIN_B, D to BUTTON_PIN.
//

#include "iotsa.h"
#include "iotsaInput.h"
#include "iotsaDimmerMod.h"

// The button is the board's BOOT button, so it works without extra hardware.
#if defined(ESP8266)
#define BUTTON_PIN 0
#define ENCODER_PIN_A 14
#define ENCODER_PIN_B 12
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BUTTON_PIN 9
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#else
#define BUTTON_PIN 0
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Dimmer Server");

// The controls: a button that acts when pressed (true), not when released
// (false), and a rotary encoder.
Button button(BUTTON_PIN, true, false);
RotaryEncoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);

// The input module reads all controls in its list.
Input* inputs[] = { &button, &encoder };
IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

// Our module, see iotsaDimmerMod.cpp. It gets the controls from here.
IotsaDimmerMod dimmerMod(application, button, encoder);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
