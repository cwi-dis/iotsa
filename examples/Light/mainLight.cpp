//
// A "light" server, which reads the light level with an LDR. Connect the
// LDR between ADC and 3.3v, with a pulldown resistor to earth (2k2 seemes to work
// well for the LDR I had. Use a value that gets close to zero readings for darkness,
// and needs a pretty bright light to make it go to the maximum value of 1024).
//
// The light level can be read through a web user interface.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"
#include "iotsaLightMod.h"

IotsaApplication application("Iotsa Light Server");
IotsaWifiMod wifiMod(application);
IotsaOtaMod otaMod(application);
IotsaLightMod lightMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
