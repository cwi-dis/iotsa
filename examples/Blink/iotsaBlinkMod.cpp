#include "iotsaBlinkMod.h"

// Implementation of the Blink module. It is built like the Hello module in
// HelloIotsa (read that one first), with real hardware instead of a name:
// an output pin, and one setting, the interval, which is not saved.

void IotsaBlinkMod::setup() {
  pinMode(pin, OUTPUT);
}

void IotsaBlinkMod::lateSetup() {
  api.setup("blink", true, true);
  name = "blink";
}

// Arduino's Blink does digitalWrite(), delay(1000), digitalWrite(), delay(1000).
// In iotsa you must never wait like that: while loop() waits, the device
// doesn't answer web pages or REST calls, and WiFi and over-the-air updates
// stop working. So instead loop() looks at the clock, and only does something
// when it is time to switch the LED. Most calls return straight away.
void IotsaBlinkMod::loop() {
  if (interval <= 0) return;  // 0: stop blinking, the LED stays as it is
  if (millis() - lastToggle < (unsigned long)interval) return;
  lastToggle = millis();
  ledOn = !ledOn;
  // On some boards the LED lights when the pin is LOW. For blinking that
  // doesn't matter.
  digitalWrite(pin, ledOn ? HIGH : LOW);
}

// No configLoad()/configSave(), on purpose: unlike HelloIotsa's name, the
// interval is not saved. After a reboot it is 1000 ms again. The fields are
// Live (below), which tells the web page not to save anything either.

bool IotsaBlinkMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  JsonObject reqObj = request.as<JsonObject>();
  // Nothing to save, so no configSave() and *changed stays as it is.
  getFromRequest<int>(reqObj, "interval", interval);
  checkUnhandled(reqObj);
  return true;
}

bool IotsaBlinkMod::getHandler(const char *path, JsonObject& reply) {
  reply["interval"] = interval;
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaBlinkMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("interval", interval, IotsaWebContext::Live);
}

void IotsaBlinkMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  IotsaWebContext::Form form(ctx, "Set");
  ctx.showField("Interval (ms)", "interval", interval, IotsaWebContext::Live, "0 stops blinking");
}
#endif // IOTSA_WITH_WEB

String IotsaBlinkMod::info() {
  String rv = "<p>The LED blinks every ";
  rv += String(interval);
  rv += " ms. See <a href=\"/blink\">/blink</a> to change.</p>";
  return rv;
}
