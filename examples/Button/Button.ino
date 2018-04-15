//
// Server for a button. When the button is pressed a configurable http[s] request is sent.
// A led is flashed red or green to indicate success or failure.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"
#include "iotsaLed.h"
#include "iotsaCapabilities.h"
#include "iotsaButton.h"
#include <functional>

#define PIN_BUTTON 4	// GPIO4 is the pushbutton
#define PIN_NEOPIXEL 15  // pulled-down during boot, can be used for NeoPixel afterwards

ESP8266WebServer server(80);
IotsaApplication application(server, "Button Server");

// Configure modules we need
IotsaWifiMod wifiMod(application);  // wifi is always needed
IotsaOtaMod otaMod(application);    // we want OTA for updating the software (will not work with esp-201)
IotsaLedMod ledMod(application, PIN_NEOPIXEL);

Button buttons[] = {
  Button(PIN_BUTTON, true, false)
};
const int nButton = sizeof(buttons) / sizeof(buttons[0]);
callback buttonOk = std::bind(&IotsaLedMod::set, ledMod, 0x002000, 250, 0, 1);
callback buttonNotOk = std::bind(&IotsaLedMod::set, ledMod, 0x200000, 250, 0, 1);

IotsaButtonMod buttonMod(application, buttons, nButton, &myTokenAuthenticator, buttonOk, buttonNotOk);

//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.serverSetup();
}
 
void loop(void) {
  application.loop();
} 
