#include "iotsaButton.h"
#include "iotsaConfigFile.h"

#define DEBOUNCE_DELAY 50 // 50 ms debouncing

void IotsaButtonMod::configLoad() {
  IotsaConfigFileLoad cf("/config/buttons.cfg");
  for (int i=0; i<nButton; i++) {
    String name = "button" + String(i+1);
    buttons[i].req.configLoad(cf, name);
    name = name + "on";
    String sendOn;
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
    } else {
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

void IotsaButtonMod::setup() {
  for (int i=0; i<nButton; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
  configLoad();
}

#ifdef IOTSA_WITH_WEB
void IotsaButtonMod::handler() {
  bool any = false;
  // xxxjack do this different with args like button2.on, etc
  for (uint8_t i=0; i<server->args(); i++){
    for (int j=0; j<nButton; j++) {
      if (buttons[j].req.formHandler_args(server, "button" + String(j+1), true)) {
          any = true;
      }
      String wtdName = "button" + String(j+1) + "on";
      if (server->hasArg(wtdName)) {
        String arg = server->arg(wtdName);
        if (arg == "press") {
          buttons[j].sendOnPress = true;
          buttons[j].sendOnRelease = false;
        } else if (arg == "release") {
          buttons[j].sendOnPress = false;
          buttons[j].sendOnRelease = true;
        } else if (arg == "change") {
          buttons[j].sendOnPress = true;
          buttons[j].sendOnRelease = true;
        } else {
          buttons[j].sendOnPress = false;
          buttons[j].sendOnRelease = false;
        }
        any = true;
      }
    }
  }
  if (any) configSave();

  String message = "<html><head><title>LCD Server Buttons</title></head><body><h1>LCD Server Buttons</h1>";
  for (int i=0; i<nButton; i++) {
    message += "<p>Button " + String(i+1) + ": ";
    if (buttons[i].buttonState) message += "on"; else message += "off";
    message += "</p>";
  }
  message += "<form method='get'>";
  for (int i=0; i<nButton; i++) {
    buttons[i].req.formHandler_fields(message, "Button " + String(i+1), "button" + String(i+1), true);
    message += "Call URL on: ";
    message += "<input name='button" + String(i+1) + "on' type='radio' value='press'";
    if (buttons[i].sendOnPress && !buttons[i].sendOnRelease) message += " checked";
    message += "> Press ";
    
    message += "<input name='button" + String(i+1) + "on' type='radio' value='release'";
    if (!buttons[i].sendOnPress && buttons[i].sendOnRelease) message += " checked";
    message += "> Release ";
    
    message += "<input name='button" + String(i+1) + "on' type='radio' value='change'";
    if (buttons[i].sendOnPress && buttons[i].sendOnRelease) message += " checked";
    message += "> Press and release ";
    
    message += "<input name='button" + String(i+1) + "on' type='radio' value='none'";
    if (!buttons[i].sendOnPress && !buttons[i].sendOnRelease) message += " checked";
    message += "> Never<br>\n";
  }
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaButtonMod::info() {
  return "<p>See <a href='/buttons'>/buttons</a> to program URLs for button presses, <a href='/api/buttons'>/api/buttons</a> for REST interface..</a>";
}
#endif // IOTSA_WITH_WEB

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

#ifdef IOTSA_WITH_API
bool IotsaButtonMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/buttons") == 0) {
    JsonArray rv = reply["buttons"].to<JsonArray>();
    for (Button *b=buttons; b<buttons+nButton; b++) {
        JsonObject bRv = rv.add<JsonObject>();
        b->req.getHandler(bRv);
        bRv["state"] = b->buttonState;
        bRv["onPress"] = b->sendOnPress;
        bRv["onRelease"] = b->sendOnRelease;
    }
  } else {
      String num(path);
      num.remove(0, 13);
      int idx = num.toInt();
      Button *b = buttons + idx;
      b->req.getHandler(reply);
      reply["state"] = b->buttonState;
      reply["onPress"] = b->sendOnPress;
      reply["onRelease"] = b->sendOnRelease;
  }
  return true;
}

bool IotsaButtonMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  if (strcmp(path, "/api/buttons") == 0) {
      if (!request.is<JsonArray>()) {
        return false;
      }
      const JsonArray all = request.as<JsonArray>();
      for (int i=0; i<nButton; i++) {
          const JsonVariant r = all[i];
          if (buttons[i].req.putHandler(r)) {
              anyChanged = true;
          }
          const JsonObject reqObj = r.as<JsonObject>();
          if (getFromRequest<int>(reqObj, "onPress", buttons[i].sendOnPress)) {
            anyChanged = true;
          }
          if (getFromRequest<int>(reqObj, "onRelease", buttons[i].sendOnRelease)) {
            anyChanged = true;
          }          
      }
  } else {
      String num(path);
      num.remove(0, 13);
      int idx = num.toInt();
      Button *b = buttons + idx;
      if (b->req.putHandler(request)) {
        anyChanged = true;
      }
      const JsonObject reqObj = request.as<JsonObject>();
      if (getFromRequest<int>(reqObj, "onPress", buttons[idx].sendOnPress)) {
        anyChanged = true;
      }
      if (getFromRequest<int>(reqObj, "onRelease", buttons[idx].sendOnRelease)) {
        anyChanged = true;
      }          
  }
  // xxxjack cannot use checkUnhandled() because of the array of buttons
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaButtonMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/buttons", std::bind(&IotsaButtonMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/buttons", true, true);
  for(int i=0; i<nButton; i++) {
      String *p = new String("/api/buttons/" + String(i));
      api.setup(p->c_str(), true, true);
  }
  name = "buttons";
#endif
}
