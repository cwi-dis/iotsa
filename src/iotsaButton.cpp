#include "iotsaButton.h"
#include "iotsaConfigFile.h"

#define DEBOUNCE_DELAY 50 // 50 ms debouncing

void IotsaButtonMod::setup() {
  for (int i=0; i<nButton; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
  configLoad();
}

void IotsaButtonMod::lateSetup() {
  api.setup("buttons", true, true);
  name = "buttons";
}

void IotsaButtonMod::loop() {
  for (int i=0; i<nButton; i++) {
    int state = digitalRead(buttons[i].pin);
    if (state != buttons[i].debounceState) {
      buttons[i].debounceTime = millis();
    }
    buttons[i].debounceState = state;
    if (millis() > buttons[i].debounceTime + DEBOUNCE_DELAY) {
      int newButtonState = (state == LOW);
      if (newButtonState != buttons[i].buttonState) {
        buttons[i].buttonState = newButtonState;
        bool doSend = (buttons[i].buttonState && buttons[i].sendOnPress) || (!buttons[i].buttonState && buttons[i].sendOnRelease);
        if (doSend && buttons[i].req.url != "") {
          if (buttons[i].req.send()) {
            if (successCallback) successCallback();
          } else {
            if (failureCallback) failureCallback();
          }
        }
      }
    }
  }
}

void IotsaButtonMod::configLoad() {
  IotsaConfigFileLoad cf("/config/buttons.cfg");
  for (int i=0; i<nButton; i++) {
    String name = "button" + String(i+1);
    buttons[i].req.configLoad(cf, name);
    name = name + "on";
    String sendOn;
    // Not saved yet: keep what the constructor said.
    cf.get(name, sendOn, "");
    if (sendOn == "press") {
      buttons[i].sendOnPress = true;
      buttons[i].sendOnRelease = false;
    } else
    if (sendOn == "release") {
      buttons[i].sendOnPress = false;
      buttons[i].sendOnRelease = true;
    } else
    if (sendOn == "change") {
      buttons[i].sendOnPress = true;
      buttons[i].sendOnRelease = true;
    } else
    if (sendOn == "none") {
      buttons[i].sendOnPress = false;
      buttons[i].sendOnRelease = false;
    }
  }
}

void IotsaButtonMod::configSave() {
  IotsaConfigFileSave cf("/config/buttons.cfg");
  for (int i=0; i<nButton; i++) {
    String name = "button" + String(i+1);
    buttons[i].req.configSave(cf, name);
    name = name + "on";
    if (buttons[i].sendOnPress) {
      if (buttons[i].sendOnRelease) {
        cf.put(name, "change");
      } else {
        cf.put(name, "press");
      }
    } else {
      if (buttons[i].sendOnRelease) {
        cf.put(name, "release");
      } else {
        cf.put(name, "none");
      }
    }
  }
}

// An array with one entry per button, by position (cwi-dis/iotsa#217); null
// or missing entries leave that button alone.
// An array with an element per button; null leaves that button alone.
void IotsaButtonMod::putHandler(IotsaApiContext& ctx) {
  JsonVariant body = ctx.requestVariant();
  if (!body.is<JsonArray>()) {
    ctx.error(400, "expected an array of buttons");
    return;
  }
  JsonArray all = body.as<JsonArray>();
  for (int i=0; i<nButton; i++) {
    JsonVariant r = all[i];
    if (r.isNull()) continue;
    if (!r.is<JsonObject>()) {
      ctx.error(400, "expected an object for each button");
      return;
    }
    IotsaApiContext::Scope s(ctx, String(i), r.as<JsonObject>(), JsonObject());
    buttons[i].req.putHandler(ctx);
    ctx.arg("onPress", buttons[i].sendOnPress, IotsaApiContext::Saved);
    ctx.arg("onRelease", buttons[i].sendOnRelease, IotsaApiContext::Saved);
  }
}

void IotsaButtonMod::getHandler(IotsaApiContext& ctx) {
  JsonArray rv = ctx.reply()["buttons"].to<JsonArray>();
  for (int i=0; i<nButton; i++) {
    IotsaApiContext::Scope s(ctx, String(i), JsonObject(), rv.add<JsonObject>());
    buttons[i].req.getHandler(ctx);
    ctx.show("state", buttons[i].buttonState);
    ctx.show("onPress", buttons[i].sendOnPress);
    ctx.show("onRelease", buttons[i].sendOnRelease);
  }
}

#ifdef IOTSA_WITH_WEB
// "Call URL on" as a choice: bit 0 press, bit 1 release.
static const char * const sendOnNames[] = {"Never", "Press", "Release", "Press and release"};

void IotsaButtonMod::formHandler_args(IotsaWebContext& ctx) {
  for (int i=0; i<nButton; i++) {
    IotsaWebContext::Scope s(ctx, "button" + String(i+1));
    buttons[i].req.formHandler_args(ctx);
    int sendOn = (buttons[i].sendOnPress ? 1 : 0) | (buttons[i].sendOnRelease ? 2 : 0);
    if (ctx.argChoice("on", sendOn, 4, IotsaWebContext::Saved)) {
      buttons[i].sendOnPress = (sendOn & 1) != 0;
      buttons[i].sendOnRelease = (sendOn & 2) != 0;
    }
  }
}

void IotsaButtonMod::formHandler_fields(IotsaWebContext& ctx) {
  for (int i=0; i<nButton; i++) {
    String label = "Button " + String(i+1);
    IotsaWebContext::Scope s(ctx, "button" + String(i+1));
    if (ctx.includesEveryday()) {
      ctx.showStatus(label, "state", buttons[i].buttonState ? "pressed" : "released");
    } else {
      IotsaWebContext::Form form(ctx, "Save", label);
      buttons[i].req.formHandler_fields(ctx);
      int sendOn = (buttons[i].sendOnPress ? 1 : 0) | (buttons[i].sendOnRelease ? 2 : 0);
      ctx.showChoice("Call URL on", "on", sendOn, sendOnNames, 4, IotsaWebContext::Saved);
    }
  }
}

String IotsaButtonMod::info() {
  return "<p>See <a href='/buttons'>/buttons</a> to program URLs for button presses, <a href='/api/buttons'>/api/buttons</a> for REST interface.</p>";
}
#endif // IOTSA_WITH_WEB
