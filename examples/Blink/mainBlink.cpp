//
// Blink, the iotsa way: blinks an LED, like the Arduino Blink example, but you
// can change how fast on the web page /blink or with PUT /api/blink. The
// setting is not kept over reboots.
//
// Like Arduino's Blink this uses LED_BUILTIN, the pin of the board's own LED
// (if it has one; check your board, or connect an LED with a resistor to that
// pin). To use another pin, build with -DBLINK_PIN=<n>.
//
// On some boards the LED is iotsa's status LED (the board defines
// IOTSA_PIN_LED). Then build with -DIOTSA_WITHOUT_STATUS_LED, so iotsa leaves
// the pin alone. iotsa-build.json does this for the ESP32-C3 SuperMini.
//

#include "iotsa.h"
#include "iotsaBlinkMod.h"

#ifndef BLINK_PIN
#define BLINK_PIN LED_BUILTIN
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Blink Server");

// Our module, see iotsaBlinkMod.cpp. It gets the pin from here, so the module
// itself doesn't need to know about boards.
IotsaBlinkMod blinkMod(application, BLINK_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
