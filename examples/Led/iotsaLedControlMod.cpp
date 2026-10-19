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
// The page triggers a transient status-LED pulse (cwi-dis/iotsa#176) with the
// requested color/timing. It always decays back to the normal status display
// on its own. The pulse happens now, so it is an action, and the fields are
// its parameters.
void IotsaLedControlMod::formHandler_args(IotsaWebContext& ctx) {
  if (!ctx.action("pulse")) return;
  String rgb = "ffffff";
  uint32_t onDuration = 0;
  uint32_t offDuration = 0;
  uint32_t durationMs = 1000;
  ctx.arg("rgb", rgb, IotsaWebContext::Live);
  ctx.arg("onDuration", onDuration, IotsaWebContext::Live);
  ctx.arg("offDuration", offDuration, IotsaWebContext::Live);
  ctx.arg("durationMs", durationMs, IotsaWebContext::Live);
  iotsaStatus.setStatusPulse(strtol(rgb.c_str(), 0, 16), onDuration, offDuration, durationMs, "web led control");
  ctx.feedback("Pulsing.");
}

void IotsaLedControlMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  IotsaWebContext::Form form(ctx, "Pulse", "Status LED pulse", "pulse");
  ctx.field("Color", "rgb", "ffffff", IotsaWebContext::Live, "hex rrggbb");
  ctx.field("On time", "onDuration", 0, IotsaWebContext::Live, "ms, 0: steady");
  ctx.field("Off time", "offDuration", 0, IotsaWebContext::Live, "ms");
  ctx.field("Total duration", "durationMs", 1000, IotsaWebContext::Live, "ms");
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

bool IotsaLedControlMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  uint32_t _rgb = request["rgb"]|0xffffff;
  uint32_t _onDuration = request["onDuration"]|0;
  uint32_t _offDuration = request["offDuration"]|0;
  uint32_t _durationMs = request["durationMs"]|1000;
  iotsaStatus.setStatusPulse(_rgb, _onDuration, _offDuration, _durationMs, "REST led control");
  return true;
}

void IotsaLedControlMod::lateSetup() {
  name = "led";
  // Setup the web server hooks for this module. No GET: a pulse is transient,
  // there's nothing meaningful to read back (cwi-dis/iotsa#256).
  api.setup("led", false, true);
}
