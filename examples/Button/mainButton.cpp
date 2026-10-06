//
// Server for a button. When the button is pressed a configurable http[s] request is sent.
// A led is flashed red or green to indicate success or failure.
//

#include "iotsa.h"
#include "iotsaButton.h"

#ifndef BUTTON_PIN
#define BUTTON_PIN 4	// GPIO4 is the pushbutton
#endif

IotsaApplication application("Button Server");

// Configure modules we need

// Feedback goes to the board's status LED, created automatically when the
// board has one (cwi-dis/iotsa#272).

Button buttons[] = {
  Button(BUTTON_PIN, true, false)
};
const int nButton = sizeof(buttons) / sizeof(buttons[0]);
// Transient status-LED pulses (cwi-dis/iotsa#176/#256) -- decay back to the
// normal status display on their own, no restore logic needed.
callback buttonOk = []() { iotsaStatus.setStatusPulse(0x002000, 0, 0, 250, "button ok"); };
callback buttonNotOk = []() { iotsaStatus.setStatusPulse(0x200000, 0, 0, 250, "button not ok"); };

IotsaButtonMod buttonMod(application, buttons, nButton, buttonOk, buttonNotOk);

//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.lateSetup();
}
 
void loop(void) {
  application.loop();
} 
