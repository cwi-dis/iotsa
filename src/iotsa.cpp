#include <Esp.h>
#include "iotsa.h"
#include "iotsaFS.h"

// There is an issue with the platformio library dependency finder, and it doesn't find the
// esp8266httpclient library. This is a workaround.
#include "iotsaRequest.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;

IotsaApplication* IotsaApplication::applicationPtr = nullptr;
IotsaAuthMod* IotsaApplication::authenticatorPtr = nullptr;

IotsaApplication::IotsaApplication(const char *_title)
: IotsaWebServerMixin(this),
  status(NULL),
  firstModule(NULL), 
  firstEarlyModule(NULL), 
  title(_title),
  haveOTA(false)
{
  if (applicationPtr != nullptr) {
    IotsaSerial.println("IotsaApplication() should only be called once");
  }
  applicationPtr = this;
}

void
IotsaApplication::addMod(IotsaBaseMod *mod) {
  mod->nextModule = firstModule;
  firstModule = mod;
}

void
IotsaApplication::addModEarly(IotsaBaseMod *mod) {
  mod->nextModule = firstEarlyModule;
  firstEarlyModule = mod;
}

void
IotsaApplication::setup() {
  // xxxjack Unsure about this. We always open the Serial port,
  // so log messages that aren't flagged with IFDEBUG always work.
  // But this means the serial port cannot be used for other things.
  Serial.begin(115200);
  IFDEBUG IotsaSerial.println("Serial opened");
  IFDEBUG IotsaSerial.print("Opening " IOTSA_FS_NAME " (may take long)...");
  bool ok = IOTSA_FS.begin();
  IFDEBUG IotsaSerial.println(" done.");
  if (!ok) {
    IFDEBUG IotsaSerial.println("IOTSA_FS.begin() failed, formatting");

    ok = IOTSA_FS.format();
    if (!ok) {
      IFDEBUG IotsaSerial.println(IOTSA_FS_NAME ".format() failed");
    }
    ok = IOTSA_FS.begin();
    if (!ok) {
      IFDEBUG IotsaSerial.println(IOTSA_FS_NAME ".begin() after format failed");
    }
  } else {
    IFDEBUG IotsaSerial.println(IOTSA_FS_NAME " mounted");
  }
  // Normally iotsaConfigMod is initialized by the WiFi module,
  // but if the WiFi module isn't indluded we ensure that the configuration file is loaded anyway.
  iotsaConfig.ensureConfigLoaded();
  
  IotsaBaseMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->setup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->setup();
  }
#ifndef ESP32
  ESP.wdtEnable(WDTO_120MS);
#endif
  IFDEBUG IotsaSerial.print("hostname: ");
  IFDEBUG IotsaSerial.println(iotsaConfig.hostName);
}

void
IotsaApplication::serverSetup() {
  // xxxjack this is wrong: if (!iotsaConfig.wifiEnabled) return;
  IotsaBaseMod *m;

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  webServerSetup();
#endif

  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
}

void
IotsaApplication::loop() {
  iotsaConfig.loop();
  IotsaBaseMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->loop();
  }
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  webServerLoop();
#endif
#ifdef ESP32
  {
    // Print available free heap space first time we have gone through all loop() calls.
    static bool once = false;
    if (!once) {
      iotsaConfig.printHeapSpace();
      once = true;
    }
  }
#endif // ESP32
}

#ifdef IOTSA_WITH_WEB
String IotsaBaseMod::info() {
  // Info method that does nothing, usually overridden for IotsaMod modules
  return "";
}

String IotsaMod::htmlEncode(String data) {
  const char *p = data.c_str();
  String rv = "";
  while(p && *p) {
    char escapeChar = *p++;
    switch(escapeChar) {
      case '&': rv += "&amp;"; break;
      case '<': rv += "&lt;"; break;
      case '>': rv += "&gt;"; break;
      case '"': rv += "&quot;"; break;
      case '\'': rv += "&#x27;"; break;
      case '/': rv += "&#x2F;"; break;
      default: rv += escapeChar; break;
    }
  }
  return rv;
}

//
// Decode percent-escaped string src.
// 
void IotsaMod::percentDecode(const String &src, String &dst) {
    const char *arg = src.c_str();
    dst = String();
    while (*arg) {
      char newch = 0;
      if (*arg == '+') newch = ' ';
      else if (*arg == '%') {
        arg++;
        if (*arg == 0) break;
        if (*arg >= '0' && *arg <= '9') newch = (*arg-'0') << 4;
        if (*arg >= 'a' && *arg <= 'f') newch = (*arg-'a'+10) << 4;
        if (*arg >= 'A' && *arg <= 'F') newch = (*arg-'A'+10) << 4;
        arg++;
        if (*arg == 0) break;
        if (*arg >= '0' && *arg <= '9') newch |= (*arg-'0');
        if (*arg >= 'a' && *arg <= 'f') newch |= (*arg-'a'+10);
        if (*arg >= 'A' && *arg <= 'F') newch |= (*arg-'A'+10);
      } else {
        newch = *arg;
      }
      dst += newch;
      arg++;
    }
}
#endif // IOTSA_WITH_WEB

bool IotsaBaseMod::needsAuthentication(const char *object, IotsaApiOperation verb) { 
  return auth ? !auth->allows(object, verb) : false; 
}

bool IotsaBaseMod::needsAuthentication(const char *right) { 
  return auth ? !auth->allows(right) : false; 
}

void IotsaBaseMod::serverSetup() {
  // setup method that does nothing, usually overridden for IotsaMod modules
}
