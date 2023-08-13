//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// The server always includes the Wifi configuration module. You can enable
// other modules with the preprocessor defines. With the default defines the server
// will allow serving of web pages and other documents, and of uploading those.
//

#include <Arduino.h>
#include "iotsa.h"
#include "iotsaWifi.h"

// CHANGE: Add application includes and declarations here

#undef WITH_USER   // Enable username/password authentication for changing configurations
//#define IOTSA_WITH_NTP    // Use network time protocol to synchronize the clock.
//#define IOTSA_WITH_FILES  // Enable static files webserver
//#define IOTSA_WITH_FILESUPLOAD  // Enable upload of static files for webserver
//#define IOTSA_WITH_FILESBACKUP  // Enable backup of all files including config files and webserver files
//#define IOTSA_WITH_BATTERY // Enable power-saving support

IotsaApplication application("Iotsa Input Server");
#ifdef WITH_USER
#include "iotsaUser.h"
IotsaUserMod userMod(application);
#endif

#include "iotsaStandardModules.h"

#include "iotsaInput.h"
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

#include "iotsaStandardModules.h"

#ifdef IOTSA_WITH_BATTERY
#define PIN_DISABLE_SLEEP 0 // Define for pin on which low signal disables sleep
#endif

void setup(void){
  application.setup();
  application.serverSetup();
#ifdef PIN_DISABLE_SLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLE_SLEEP);
#endif
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

