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

static int name2mode(String &name) {
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
  AnalogPort("a0", A0),
};
#define nDigital (sizeof(digital)/sizeof(digital[0]))

void
IotsaSimpleIOMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++) {
    
    for (int pi=0; pi<nDigital; pi++) {
      GPIOPort &p = digital[pi];
      if (server.argName(i) == p.name + "mode") {
        String sVal = server.arg(i);
        int val = name2mode(sVal);
        if (val != p.mode) {
          p.mode = val;
          anyChanged = true;
        }
      } else
      if (server.argName(i) == p.name + "value") {
        int val = server.arg(i);
      }
    }
    if( server.argName(i) == "argument") {
    	if (needsAuthentication()) return;
    	argument = server.arg(i);
    	anyChanged = true;
      c
    }
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Simple GPIO module</title></head><body><h1>Simple GPIO</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  message += htmlEncode(argument);
  message += "'><br><input type='submit'></form>";
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
