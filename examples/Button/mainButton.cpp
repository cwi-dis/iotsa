//
// A "Button" server: when the button is pressed, it calls a URL that you set
// on the web page /buttons or with PUT /api/buttons. The status LED flashes
// green when the call worked, red when it didn't.
//
// Together with the Ringer example this is a doorbell: set the button's URL
// to the ringer's trigger, http://ringer.local/ring?alarm=20.
//
// The button module is a standard iotsa module (iotsaButton.h), so there is
// no module of our own here.
//

#include "iotsa.h"
#include "iotsaButton.h"

#ifndef BUTTON_PIN
#define BUTTON_PIN 4  // GPIO4: a pushbutton to ground
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Button Server");

// The buttons: here only one. Calls its URL when pressed (true), not when
// released (false). Both can be changed on the web page.
Button buttons[] = {
  Button(BUTTON_PIN, true, false)
};
const int nButton = sizeof(buttons) / sizeof(buttons[0]);

// What to do when the call worked, or didn't: flash the status LED for a
// quarter of a second, green or red.
callback buttonOk = []() { iotsaStatus.setStatusPulse(0x002000, 0, 0, 250, "button ok"); };
callback buttonNotOk = []() { iotsaStatus.setStatusPulse(0x200000, 0, 0, 250, "button not ok"); };

// The button module, with the buttons and what to do afterwards.
IotsaButtonMod buttonMod(application, buttons, nButton, buttonOk, buttonNotOk);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
