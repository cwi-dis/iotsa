//
// A "BLE Button" server: the Button example, but it rings a BLERinger over
// BLE instead of calling a URL. Set the ringer's name on the web page
// /doorbell or with PUT /api/doorbell. See iotsaBLEButtonMod.cpp.
//
// Ringing uses the Immediate Alert Service, a Bluetooth standard (the "Find
// Me" profile), so a generic BLE tool such as nRF Connect can ring a
// BLERinger too.
//
// By default the button is the BOOT button of an ESP32-C3 (GPIO9), so you
// need no extra hardware. Don't hold it while the board resets: then it
// starts the bootloader.
//

#include "iotsa.h"
#include "iotsaInput.h"
#include "iotsaBLEClient.h"
#include "iotsaBLEButtonMod.h"

#ifndef IOTSA_WITH_BLE
#error This example needs BLE: build with -DIOTSA_WITH_BLE
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN 9  // BOOT button on the ESP32-C3
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("BLE Doorbell Button");

// The BLE client module: finds other devices over BLE and connects to them.
IotsaBLEClientMod bleClientMod(application);

// The button: acts when pressed (true), not when released (false).
Button doorbellButton(BUTTON_PIN, true, false);
Input *inputs[] = { &doorbellButton };
IotsaInputMod inputMod(application, inputs, 1);

// Our module, see iotsaBLEButtonMod.cpp. It gets the BLE client module and
// the button from here.
IotsaBLEButtonMod buttonMod(application, bleClientMod, doorbellButton);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
