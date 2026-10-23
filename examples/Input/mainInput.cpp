//
// A dimmer controlled by hand: a pushbutton switches an LED on and off, a
// rotary encoder sets its brightness. It shows the Input module (iotsaInput.h),
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
// The LED is the board's own LED, except on the C3 and S3: there that is the
// status LED, a NeoPixel, which PWM can't dim. Connect an LED with a
// resistor (330 ohm) between DIMMER_PIN and ground instead.
#if defined(ESP8266)
#define BUTTON_PIN 0
#define ENCODER_PIN_A 14
#define ENCODER_PIN_B 12
#define DIMMER_PIN LED_BUILTIN
#define DIMMER_ACTIVE_LOW true   // the LED lights when the pin is LOW
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BUTTON_PIN 9
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#define DIMMER_PIN 6
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define BUTTON_PIN 0
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#define DIMMER_PIN 6
#else
#define BUTTON_PIN 0
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#define DIMMER_PIN LED_BUILTIN
#endif
#ifndef DIMMER_ACTIVE_LOW
#define DIMMER_ACTIVE_LOW false
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

// Our module, see iotsaDimmerMod.cpp. It gets the controls and the LED from here.
IotsaDimmerMod dimmerMod(application, button, encoder, DIMMER_PIN, DIMMER_ACTIVE_LOW);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
