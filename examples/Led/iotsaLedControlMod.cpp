#include "iotsaLedControlMod.h"

#ifdef IOTSA_WITH_BLE
void IotsaLedControlMod::setup() {
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(rgbUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT32, 0x2700, "RGBx color");
}

bool IotsaLedControlMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      uint32_t _rgb = bleApi.getAsInt(rgbUUID);
      // BLE always sets a solid, non-repeating color -- see the header comment.
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
IotsaLedControlMod::webHandler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  int _count = 1;
  int _onDuration = 0;
  int _offDuration = 0;
  if( api.webService->server->hasArg("rgb")) {
    _rgb = strtol(api.webService->server->arg("rgb").c_str(), 0, 16);
    anyChanged = true;
  }
  if( api.webService->server->hasArg("onDuration")) {
    _onDuration = api.webService->server->arg("onDuration").toInt();
    anyChanged = true;
  }
  if( api.webService->server->hasArg("offDuration")) {
    _offDuration = api.webService->server->arg("offDuration").toInt();
    anyChanged = true;
  }
  if( api.webService->server->hasArg("count")) {
    _count = api.webService->server->arg("count").toInt();
    anyChanged = true;
  }
  if (anyChanged) set(_rgb, _onDuration, _offDuration, _count);

  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb' value='" + String(rgb, HEX) + "'><br>";
  message += "On time (ms): <input type='text' name='onDuration'><br>";
  message += "Off time (ms): <input type='text' name='offDuration'><br>";
  message += "Repeat count: <input type='text' name='count'><br>";
  message += "<input type='submit'></form></body></html>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for flashing the led in a color pattern.";
#ifdef IOTSA_HAS_RESTSERVER
  rv += " Or use REST api at <a href='/api/led'>/api/led</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + " for setting a solid color.";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
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
#endif // IOTSA_WITH_API

void IotsaLedControlMod::lateSetup() {
  name = "led";
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_API
  api.setup("led", true, true);
#endif
}
