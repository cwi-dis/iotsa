//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
// A "Led" module is added, which allows control over a single NeoPixel (color,
// duration, on/off pattern).
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

ESP8266WebServer server(80);
IotsaApplication application(server, "Iotsa LED Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// LED module. 
//

#define NEOPIXEL_PIN 4

class IotsaLedControlMod : public IotsaLedMod {
public:
  using IotsaLedMod::IotsaLedMod;
  void serverSetup();
  String info();
private:
  void handler();
};


void
IotsaLedControlMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  LED digitalWrite(led, 1);
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  int _count = 1;
  int _onDuration = 0;
  int _offDuration = 0;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "rgb") {
      _rgb = strtol(server.arg(i).c_str(), 0, 16);
      anyChanged = true;
    }
    if( server.argName(i) == "onDuration") {
      _onDuration = server.arg(i).toInt();
      anyChanged = true;
    }
    if( server.argName(i) == "offDuration") {
      _offDuration = server.arg(i).toInt();
      anyChanged = true;
    }
    if( server.argName(i) == "count") {
      _count = server.arg(i).toInt();
      anyChanged = true;
    }
  }
  if (anyChanged) set(_rgb, _onDuration, _offDuration, _count);
  
  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb'><br>";
  message += "On time (ms): <input type='text' name='onDuration'><br>";
  message += "Off time (ms): <input type='text' name='offDuration'><br>";
  message += "Repeat count: <input type='text' name='count'><br>";
  message += "<input type='submit'></form></body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void IotsaLedControlMod::serverSetup() {
  // Setup the web server hooks for this module.
  server.on("/led", std::bind(&IotsaLedControlMod::handler, this));
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for flashing the led in a color pattern.</p>";
  return rv;
}

// Instantiate the Led module, and install it in the framework
IotsaLedControlMod ledMod(application, NEOPIXEL_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

