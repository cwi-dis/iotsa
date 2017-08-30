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
AnalogInput a0("a0", A0);
GPIOPort *ports[] = {
  &io4, &io5, &io12, &io13, &io14, &io16, &a0
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

  // See if we want json
  if (server.arg("json") != "") {
    String json = "{}";
    server.send(200, "application/json", json);
    return;
  }
  String message = "<html><head><title>Simple GPIO module</title></head><body><h1>Simple GPIO</h1>";
  message += "<form method='get'><table><tr><th>Pin</th><th>Mode</th><th>Value</th></tr>";
  for (GPIOPort **pp=ports; pp < &ports[nPorts]; pp++) {
    GPIOPort *p = *pp;
    message += "<tr><td>" + p->name + "</td>";
    message += "<td><input name='" + p->name + "mode' value='" + mode2name(p->getMode()) + "'></td>";
    message += "<td><input name='" + p->name + "value' value='" + String(p->getValue()) + "'></td>";
    message += "</tr>";
  }
  message += "</table><input type='submit'></form>";
  server.send(200, "text/html", message);
}

void IotsaSimpleIOMod::setup() {
  configLoad();
}

void IotsaSimpleIOMod::serverSetup() {
  server.on("/io", std::bind(&IotsaSimpleIOMod::handler, this));
}

void IotsaSimpleIOMod::configLoad() {
  IotsaConfigFileLoad cf("/config/SimpleIO.cfg");
  cf.get("argument", argument, "");
 
}

void IotsaSimpleIOMod::configSave() {
  IotsaConfigFileSave cf("/config/SimpleIO.cfg");
  cf.put("argument", argument);
}

void IotsaSimpleIOMod::loop() {
}

String IotsaSimpleIOMod::info() {
  String message = "<p>Built with Simple IO module. See <a href=\"/io\">/io</a> to examine GPIO pins and/or change them.</p>";
  return message;
}
