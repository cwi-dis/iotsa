//
// A "Led" server: the outside world can flash the LED, with a colour, on and
// off times, and how long. On the web page /led, with PUT /api/led (REST, or
// CoAP when built with IOTSA_WITH_COAP), and, built with IOTSA_WITH_BLE, with
// a BLE service of its own. See iotsaLedControlMod.cpp.
//
// The same on a battery-powered device, with sleep: see LedBattery.
//

#include "iotsa.h"
#include "iotsaLedControlMod.h"

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa LED Server");

// Our module, see iotsaLedControlMod.cpp. It also is the status LED, so it
// needs the LED: the board's own status LED if it has one (the board defines
// IOTSA_PIN_LED), otherwise a NeoPixel whose data line you connect to GPIO15.
#ifdef IOTSA_PIN_LED
IotsaLedControlMod ledMod(application);
#else
IotsaLedControlMod ledMod(application, 15);
#endif

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
