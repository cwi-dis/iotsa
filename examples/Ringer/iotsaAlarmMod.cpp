#include "iotsaAlarmMod.h"

// Implementation of the Alarm module. It is built like the Blink module (read
// that one first), with one new thing: a trigger, see ringTrigger().

void IotsaAlarmMod::setup() {
  pinMode(pin, OUTPUT);
}

void IotsaAlarmMod::lateSetup() {
  api.setup("alarm", true, true);
  name = "alarm";
#ifdef IOTSA_WITH_WEB
  addTrigger("ring", [this](IotsaWebContext& ctx) { ringTrigger(ctx); });
#endif
}

// Stop when the time is up. millis() - alarmStart is right even when millis()
// wraps around (after 49 days); comparing millis() with an end time is not.
void IotsaAlarmMod::loop() {
  if (alarmDuration && millis() - alarmStart >= alarmDuration) _ring(0);
}

// No configLoad()/configSave(): there is nothing to keep over reboots.

// Ring for dur tenths of a second; 0 stops it.
void IotsaAlarmMod::_ring(int dur) {
  if (dur > 0) {
    alarmStart = millis();
    alarmDuration = dur*100;
    digitalWrite(pin, HIGH);
  } else {
    alarmDuration = 0;
    digitalWrite(pin, LOW);
  }
}

void IotsaAlarmMod::putHandler(IotsaApiContext& ctx) {
  // -1: not asked; 0 stops the alarm.
  int dur = -1;
  bool ring = ctx.arg("alarm", dur, IotsaApiContext::Transient);
  if (ctx.failed()) return;
  if (ring) _ring(dur);
}

// The time left, in tenths of a second.
void IotsaAlarmMod::getHandler(IotsaApiContext& ctx) {
  int dur = 0;
  unsigned long elapsed = millis() - alarmStart;
  if (alarmDuration && elapsed < alarmDuration) dur = (alarmDuration - elapsed)/100;
  ctx.show("alarm", dur);
}

#ifdef IOTSA_WITH_WEB
// Ringing happens now: Ring is an action, the duration its parameter.
void IotsaAlarmMod::formHandler_args(IotsaWebContext& ctx) {
  bool ring = ctx.argAction("ring", IotsaWebContext::Transient);
  int dur = 10;
  if (ring) ctx.arg("alarm", dur, IotsaWebContext::Transient);
  bool stop = ctx.argAction("stop", IotsaWebContext::Transient);
  if (ctx.failed()) return;
  if (ring) _ring(dur);
  if (stop) _ring(0);
}

void IotsaAlarmMod::formHandler_fields(IotsaWebContext& ctx) {
  if (!ctx.includesEveryday()) return;
  ctx.showStatus("Alarm", "alarm", alarmDuration ? "ringing" : "off");
  {
    IotsaWebContext::Form form(ctx, "Ring", "", "ring");
    ctx.showField("Duration", "alarm", 10, IotsaWebContext::Transient, "times 0.1 second");
  }
  if (alarmDuration) ctx.showButton("stop", "Stop", IotsaWebContext::Transient);
}

// The trigger, /ring: a plain URL that rings right away, for curl or another
// device. curl 'http://ringer.local/ring?alarm=20' rings for 2 seconds. The
// web page above is for people: it only rings when its Ring button was
// pressed (ring=1 in the URL). The trigger needs no such thing.
void IotsaAlarmMod::ringTrigger(IotsaWebContext& ctx) {
  int dur = 10;
  ctx.arg("alarm", dur, IotsaWebContext::Transient);
  if (ctx.failed()) return;
  _ring(dur);
  ctx.feedback("Ringing.");
}
#endif // IOTSA_WITH_WEB

String IotsaAlarmMod::info() {
  return "<p>See <a href='/alarm'>/alarm</a> to sound the buzzer, or call <a href='/ring'>/ring</a>.</p>";
}
