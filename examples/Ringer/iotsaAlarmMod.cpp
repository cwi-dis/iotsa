#include "iotsaAlarmMod.h"

void IotsaAlarmMod::setup() {
  pinMode(PIN_ALARM, OUTPUT); // Trick: we configure to input so we make the pin go Hi-Z.
}

// Ring for dur tenths of a second; 0 stops it.
void IotsaAlarmMod::_ring(int dur) {
  if (dur) {
    alarmEndTime = millis() + dur*100;
    IotsaSerial.println("alarm on");
    digitalWrite(PIN_ALARM, HIGH);
  } else {
    alarmEndTime = 0;
    digitalWrite(PIN_ALARM, LOW);
  }
}

#ifdef IOTSA_WITH_WEB
// Ringing happens now: Ring is an action, the duration its parameter.
void IotsaAlarmMod::formHandler_args(IotsaWebContext& ctx) {
  if (ctx.action("ring")) {
    int dur = 10;
    ctx.arg("alarm", dur, IotsaWebContext::Live);
    _ring(dur);
  }
  if (ctx.action("stop")) _ring(0);
}

void IotsaAlarmMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx.status("Alarm", "alarm", alarmEndTime ? "ringing" : "off");
  {
    IotsaWebContext::Form form(ctx, "Ring", "", "ring");
    ctx.field("Duration", "alarm", 10, IotsaWebContext::Live, "times 0.1 second");
  }
  if (alarmEndTime) ctx.button("stop", "Stop");
}

// The trigger, /ring: rings right away, for curl or another device (Button).
// curl 'http://ringer.local/ring?alarm=20' rings for 2 seconds.
void IotsaAlarmMod::ringTrigger(IotsaWebContext& ctx) {
  int dur = 10;
  ctx.arg("alarm", dur, IotsaWebContext::Live);
  _ring(dur);
  ctx.feedback("Ringing.");
}

String IotsaAlarmMod::info() {
  return "<p>See <a href='/alarm'>/alarm</a> to use the buzzer. REST interface on <a href='/api/alarm'>/api/alarm</a>, trigger on <a href='/ring'>/ring</a>.</p>";
}
#endif // IOTSA_WITH_WEB

bool IotsaAlarmMod::getHandler(const char *path, JsonObject& reply) {
  int dur = 0;
  if (alarmEndTime) {
    dur = (alarmEndTime - millis())/100;
  }
  reply["alarm"] = dur;
  return true;
}

bool IotsaAlarmMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  int dur = 0;
  if (request.is<int>()) {
    dur = request.as<int>();
  } else if (request.is<JsonObject>()) {
    JsonObject reqObj = request.as<JsonObject>();
    dur = reqObj["alarm"];
  } else {
    return apiError(400, "expected a duration, or {\"alarm\": duration}");
  }
  _ring(dur);
  return true;
}

void IotsaAlarmMod::lateSetup() {
  // Setup the web server hooks for this module.
  api.setup("alarm", true, true);
  name = "alarm";
#ifdef IOTSA_WITH_WEB
  addTrigger("ring", [this](IotsaWebContext& ctx) { ringTrigger(ctx); });
#endif
}

void IotsaAlarmMod::loop() {
  if (alarmEndTime && millis() > alarmEndTime) {
    alarmEndTime = 0;
    IotsaSerial.println("alarm off");
    digitalWrite(PIN_ALARM, LOW);
  }
}
