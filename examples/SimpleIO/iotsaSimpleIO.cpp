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

GPIOPort digital[] = {
  GPIOPort("io4", 4),
  GPIOPort("io5", 5),
  GPIOPort("io12", 12),
  GPIOPort("io13", 13),
  GPIOPort("io14", 14),
  GPIOPort("io16", 16),
  AnalogInput("a0", A0),
};
#define nDigital (sizeof(digital)/sizeof(digital[0]))

void
IotsaSimpleIOMod::handler() {
  // First check configuration changes
  bool anyChanged = false;
  for (int pi=0; pi<nDigital; pi++) {
    GPIOPort &p = digital[pi];
    String argName = p.name + "mode";
    if (server.hasArg(argName)) {
      int mode = name2mode(server.arg(argName));
      if (p.setMode(mode))
        anyChanged = true;
    }
  }
  if (anyChanged) configSave();

  // Now set values
  for (int pi=0; pi<nDigital; pi++) {
    GPIOPort &p = digital[pi];
    String argName = p.name + "value";
    if (server.hasArg(argName)) {
      int value = name2mode(server.arg(argName));
      p.setValue(value);
    }
  }

  // See if we want json
  if (server.arg("json") != "") {
    String json = "{}";
    server.send(200, "application/json", json);
    return;
  }
  String message = "<html><head><title>Simple GPIO module</title></head><body><h1>Simple GPIO</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  for (GPIOPort *p=digital; p < &digital[nDigital]; p++) {
    message += "<b>" + p->name + "</b>";
    message += "Mode:<input name='" + p->name + "mode' value='" + mode2name(p->getMode()) + "'>";
    message += "Value:<input name='" + p->name + "value' value='" + String(p->getValue()) + "'>";
    message += "<br>";
  }
  message += "<input type='submit'></form>";
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
