#include "iotsaLedControlMod.h"

#ifdef IOTSA_WITH_BLE
bool IotsaLedControlMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      uint32_t _rgb = bleApi.getAsInt(rgbUUID);
      set(_rgb, 1000, 0, 0x7fff);
      return true;
  }
  IotsaSerial.println("ledControlMod: ble: write unknown uuid");
  return false;
}

bool IotsaLedControlMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      bleApi.set(rgbUUID, rgb);
      return true;
  }
  IotsaSerial.println("ledControlMod: ble: read unknown uuid");
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_WITH_WEB
void
IotsaLedControlMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  if( server->hasArg("rgb")) {
    _rgb = strtol(server->arg("rgb").c_str(), 0, 16);
    anyChanged = true;
  }
  if (anyChanged) set(_rgb, 1000, 0, 0x7fff);

  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb' value='" + String(rgb, HEX) + "'><br>";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for setting the LED color.";
#ifdef IOTSA_WITH_REST
  rv += " Or use REST api at <a href='/api/led'>/api/led</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaLedControlMod::getHandler(const char *path, JsonObject& reply) {
  reply["rgb"] = rgb;
  return true;
}

bool IotsaLedControlMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  uint32_t _rgb = request["rgb"]|0xffffff;
  set(_rgb, 1000, 0, 0x7fff);
  return true;
}
#endif // IOTSA_WITH_API

void IotsaLedControlMod::serverSetup() {
  name = "led";
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/led", std::bind(&IotsaLedControlMod::handler, this));
#endif // IOTSA_WITH_WEB
#ifdef IOTSA_WITH_API
  api.setup("/api/led", true, true);
#endif
}

void IotsaLedControlMod::setup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(rgbUUID, BLE_READ|BLE_WRITE, NimBLE2904::FORMAT_UINT32, 0x2700, "RGBx color");
#endif
}
