#include "iotsaSimpleIO.h"
#include "iotsaConfigFile.h"

struct pinModeNames {
   int mode;
   String modeName;
};



struct pinModeNames pinModeNames[] = {
  {INPUT, "input"},
  {INPUT_PULLUP, "input_pullup"},
  {OUTPUT, "output"},
  {PWM_OUTPUT, "pwm_output"},
  {-1, "unused"},
};
#define nPinModeNames (sizeof(pinModeNames)/sizeof(pinModeNames[0]))

static int name2mode(const String &name) {
  for(int i=0; i<nPinModeNames; i++) {
    if (name == pinModeNames[i].modeName) return pinModeNames[i].mode;
  }
  return pinModeNames[0].mode;
}

static String& mode2name(int mode) {
  for(int i=0; i<nPinModeNames; i++) {
    if (mode == pinModeNames[i].mode) return pinModeNames[i].modeName;
  }
  return pinModeNames[0].modeName;
}

DigitalPort io4("io4", 4);
DigitalPort io5("io5", 5);
DigitalPort io12("io12", 12);
DigitalPort io13("io13", 13);
DigitalPort io14("io14", 14);
DigitalPort io16("io16", 16);
AnalogInput a0("a0", A0); // GPIO 36 on most esp32 boards, sometimes 26 or something else
TimestampInput timePort;

GPIOPort *ports[] = {
  &io4, &io5, &io12, &io13, &io14, &io16, &a0, &timePort
};
#define nPorts (sizeof(ports)/sizeof(ports[0]))

void
IotsaSimpleIOMod::handler() {
  // First check configuration changes
  bool anyChanged = false;
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    String argName = p->name + "mode";
    if (server.hasArg(argName)) {
      int mode = name2mode(server.arg(argName));
      if (p->setMode(mode))
        anyChanged = true;
    }
  }
  if (anyChanged) configSave();

  // Now set values
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    String argName = p->name + "value";
    if (server.hasArg(argName)) {
      int value = server.arg(argName).toInt();
      p->setValue(value);
    }
  }
  String message = "<html><head><title>Simple GPIO module</title></head><body><h1>GPIO Configuration</h1>";
  message += "<form method='get'><table><tr><th>Pin</th><th>Mode</th><th>Value</th></tr>";
  for (GPIOPort **pp=ports; pp < &ports[nPorts]; pp++) {
    GPIOPort *p = *pp;
    message += "<tr><td>" + p->name + "</td>";
#if 1
    int thisMode = p->getMode();
    message += "<td><select name='" + p->name + "mode'>";
    for(int i=0; i<nPinModeNames; i++) {
      message += "<option value='" + pinModeNames[i].modeName + "'";
      if (pinModeNames[i].mode == thisMode) message += " selected";
      message += ">"+pinModeNames[i].modeName+"</option>";
    }
    message += "</select></td>";
#else
    message += "<td><input name='" + p->name + "mode' value='" + mode2name(p->getMode()) + "'></td>";
#endif
    message += "<td><input name='" + p->name + "value' value='" + String(p->getValue()) + "'></td>";
    message += "</tr>";
  }
  message += "</table><input type='submit'></form>";
  server.send(200, "text/html", message);
}

void
IotsaSimpleIOMod::apiHandler() {
  // Now set values
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    String argName = p->name;
    if (server.hasArg(argName)) {
      int value = server.arg(argName).toInt();
      p->setValue(value);
    }
  }

  // See if we want json
  String json = "{";
  bool first = true;
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    if (p->getMode() == INPUT || p->getMode() == INPUT_PULLUP) {
      if (!first) json += ",";
      first = false;
      json += "\"" + p->name + "\":" + String(p->getValue());
    }
  }
  json += "}";
  server.send(200, "application/json", json);
}

void IotsaSimpleIOMod::setup() {
  configLoad();
}

void IotsaSimpleIOMod::serverSetup() {
  server.on("/ioconfig", std::bind(&IotsaSimpleIOMod::handler, this));
  server.on("/api", std::bind(&IotsaSimpleIOMod::apiHandler, this));
}

void IotsaSimpleIOMod::configLoad() {
  IotsaConfigFileLoad cf("/config/SimpleIO.cfg");
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    int mode;
    cf.get(p->name+"mode", mode, INPUT);
    p->setMode(mode);
  }
}

void IotsaSimpleIOMod::configSave() {
  IotsaConfigFileSave cf("/config/SimpleIO.cfg");
  for (int pi=0; pi<nPorts; pi++) {
    GPIOPort *p = ports[pi];
    int mode;
    cf.put(p->name+"mode", p->getMode());
  }
}

void IotsaSimpleIOMod::loop() {
}

String IotsaSimpleIOMod::info() {
  String message = "<p>Built with Simple IO module. See <a href=\"/ioconfig\">/ioconfig</a> to examine GPIO pin configuration and values, ";
  message += "<a href=\"/api\">/api</a> for JSON access to values.</p>";
  return message;
}
