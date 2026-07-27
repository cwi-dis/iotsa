//
// A "Led" server, which allows control over a single NeoPixel (color,
// duration, on/off pattern). The led can be controlled through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//
// This is the application that is usually shipped with new iotsa boards.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLedControlMod.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 15  // pulled-down during boot, can be used for NeoPixel afterwards
#endif

IotsaApplication application("Iotsa LED Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

IotsaLedControlMod ledMod(application, NEOPIXEL_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
