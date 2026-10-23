//
// A "light" server, which reads the light level with an LDR. Connect the
// LDR between ADC and 3.3v, with a pulldown resistor to earth (2k2 seems to work
// well for the LDR I had. Use a value that gets close to zero readings for darkness,
// and needs a pretty bright light to make it go to the maximum value: 1023 on
// an ESP8266, 4095 on an ESP32).
//
// The light level can be read on a web page and with GET /api/light.
//

#include "iotsa.h"
#include "iotsaLightMod.h"

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Light Server");

// Our module, see iotsaLightMod.cpp.
IotsaLightMod lightMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
