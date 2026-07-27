//
// Demonstrates the Input abstraction (iotsaInput.h): a rotary encoder and a
// pushbutton, both reporting their state changes to the serial console.
//

#include <Arduino.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaInput.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Input Sample");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

// When using an Alps EC12D rotary encoder with pushbutton here is the pinout:
// When viewed from the top there are pins at northwest, north, northeast, southwest, southeast.
// These pins are named (in Alps terminology) A, E, B, C, D.
// A and B (northwest, northeast) are the rotary encoder pins,
// C is the corresponding ground,
// D and E are the pushbutton pins.
// So, connect E and C to GND, D to GPIO0, A to GPI14, B to GPIO2
RotaryEncoder encoder(14, 2);
#define ENCODER_STEPS 20
Button button(0, true, true, true);

Input* inputs[] = {
  &button,
  &encoder
};

IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

void setup(void){
  application.setup();
  application.serverSetup();
}

int oldButtonValue = -1;
int oldEncoderValue = -1;

void loop(void){
  application.loop();
  if (button.pressed != oldButtonValue) {
    IotsaSerial.printf("button: pressed=%d\n", button.pressed);
    oldButtonValue = button.pressed;
  }
  if (encoder.value != oldEncoderValue) {
    IotsaSerial.printf("encoder: value=%d\n", encoder.value);
    oldEncoderValue = encoder.value;
  }
}

