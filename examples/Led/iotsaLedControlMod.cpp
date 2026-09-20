#include "iotsaLedControlMod.h"

#ifdef IOTSA_WITH_BLE
void IotsaLedControlMod::setup() {
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(rgbUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT32, 0x2700, "RGBx color (solid pulse)");
}

bool IotsaLedControlMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      uint32_t _rgb = bleApi.getAsInt(rgbUUID);
      iotsaStatus.setStatusPulse(_rgb, 0, 0, bleSolidDurationMs, "BLE led control");
      return true;
  }
  IotsaSerial.println("ledControlMod: ble: write unknown uuid");
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_WITH_WEB
void
IotsaLedControlMod::webHandler() {
  // Handles the page that is specific to the Led module: triggers a transient
  // status-LED pulse (cwi-dis/iotsa#176) with the requested color/timing. It
  // always decays back to the normal status display on its own.
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  uint32_t _onDuration = 0;
  uint32_t _offDuration = 0;
  uint32_t _durationMs = 1000;
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
  if( api.webService->server->hasArg("durationMs")) {
    _durationMs = api.webService->server->arg("durationMs").toInt();
    anyChanged = true;
  }
  if (anyChanged) iotsaStatus.setStatusPulse(_rgb, _onDuration, _offDuration, _durationMs, "web led control");

  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<p>Triggers a transient status-LED pulse; it decays back to the normal status display automatically.</p>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb'><br>";
  message += "On time (ms): <input type='text' name='onDuration'><br>";
  message += "Off time (ms): <input type='text' name='offDuration'><br>";
  message += "Total duration (ms): <input type='text' name='durationMs' value='1000'><br>";
  message += "<input type='submit'></form></body></html>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for triggering a status-LED pulse.";
#ifdef IOTSA_HAS_RESTSERVER
  rv += " Or use REST api at <a href='/api/led'>/api/led</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + " for a solid-color pulse.";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaLedControlMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  uint32_t _rgb = request["rgb"]|0xffffff;
  uint32_t _onDuration = request["onDuration"]|0;
  uint32_t _offDuration = request["offDuration"]|0;
  uint32_t _durationMs = request["durationMs"]|1000;
  iotsaStatus.setStatusPulse(_rgb, _onDuration, _offDuration, _durationMs, "REST led control");
  return true;
}
#endif // IOTSA_WITH_API

void IotsaLedControlMod::lateSetup() {
  name = "led";
  // Setup the web server hooks for this module. No GET: a pulse is transient,
  // there's nothing meaningful to read back (cwi-dis/iotsa#256).
#ifdef IOTSA_WITH_API
  api.setup("led", false, true);
#endif
}
