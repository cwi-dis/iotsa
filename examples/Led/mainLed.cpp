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
#include "iotsaLed.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa LED Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// LED module. 
//

//#define NEOPIXEL_PIN 4  // "Normal" pin for NeoPixel
#define NEOPIXEL_PIN 15  // pulled-down during boot, can be used for NeoPixel afterwards

class IotsaLedControlMod : public IotsaLedMod {
public:
  using IotsaLedMod::IotsaLedMod;
  void serverSetup();
  String info();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void handler();
};

#ifdef IOTSA_WITH_WEB
void
IotsaLedControlMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  int _count = 1;
  int _onDuration = 0;
  int _offDuration = 0;
  if( server->hasArg("rgb")) {
    _rgb = strtol(server->arg("rgb").c_str(), 0, 16);
    anyChanged = true;
  }
  if( server->hasArg("onDuration")) {
    _onDuration = server->arg("onDuration").toInt();
    anyChanged = true;
  }
  if( server->hasArg("offDuration")) {
    _offDuration = server->arg("offDuration").toInt();
    anyChanged = true;
  }
  if( server->hasArg("count")) {
    _count = server->arg("count").toInt();
    anyChanged = true;
  }
  if (anyChanged) set(_rgb, _onDuration, _offDuration, _count);
  
  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb'><br>";
  message += "On time (ms): <input type='text' name='onDuration'><br>";
  message += "Off time (ms): <input type='text' name='offDuration'><br>";
  message += "Repeat count: <input type='text' name='count'><br>";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for flashing the led in a color pattern.</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

bool IotsaLedControlMod::getHandler(const char *path, JsonObject& reply) {
  reply["rgb"] = rgb;
  reply["onDuration"] = onDuration;
  reply["offDuration"] = offDuration;
  reply["isOn"] = isOn;
  reply["count"] = remainingCount;
  return true;
}

bool IotsaLedControlMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  uint32_t _rgb = request["rgb"]|0xffffff;
  int _onDuration = request["onDuration"]|0;
  int _offDuration = request["offDuration"]|0;
  int _count = request["count"]|0;
  set(_rgb, _onDuration, _offDuration, _count);
  return true;
}

void IotsaLedControlMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/led", std::bind(&IotsaLedControlMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/led", true, true);
  name = "led";
}

// Instantiate the Led module, and install it in the framework
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

