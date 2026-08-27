#include <Esp.h>
#include "iotsa.h"
#include "iotsaHttpServer.h"
#include "iotsaFS.h"
#if defined(IOTSA_WITH_COAP) || defined(IOTSA_WITH_HPS)
#include "iotsaApi.h"
#endif

// There is an issue with the platformio library dependency finder, and it doesn't find the
// esp8266httpclient library. This is a workaround.
#include "iotsaRequest.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;

IotsaBaseModule::IotsaBaseModule(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool early)
: app(_app),
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  server(IotsaHttpServiceMod::serviceMod(_app) ? IotsaHttpServiceMod::serviceMod(_app)->server : nullptr),
#endif
  auth(_auth),
  nextModule(NULL)
{
  if (early) {
    app.addModEarly(this);
  } else {
    app.addMod(this);
  }
}

IotsaApplication::IotsaApplication(const char *_title)
: status(NULL),
  firstModule(NULL),
  firstEarlyModule(NULL),
  title(_title),
  haveOTA(false)
{
  // The HTTP transport has to exist before any module (early or regular) is
  // constructed, since IotsaBaseModule's own constructor (above) reads the shared
  // `server` pointer unconditionally -- see iotsaHttpServer.h. This relies on the
  // existing convention that IotsaApplication itself is declared before any module
  // in the sketch.
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaHttpServiceMod::ensureServiceMod(*this);
  server = IotsaHttpServiceMod::serviceMod(*this)->server;
#endif
}

void
IotsaApplication::addMod(IotsaBaseModule *mod) {
  mod->nextModule = firstModule;
  firstModule = mod;
}

void
IotsaApplication::addModEarly(IotsaBaseModule *mod) {
  mod->nextModule = firstEarlyModule;
  firstEarlyModule = mod;
}

void
IotsaApplication::setup() {
  // xxxjack Unsure about this. We always open the Serial port,
  // so log messages that aren't flagged with IFDEBUG always work.
  // But this means the serial port cannot be used for other things.
  Serial.begin(IOTSA_SERIAL_SPEED);
  IFDEBUG IotsaSerial.println("Serial opened");
#ifdef IOTSA_DELAY_ON_BOOT
  IFDEBUG IotsaSerial.printf("Delaying %d seconds on boot...\n", IOTSA_DELAY_ON_BOOT);
  delay(IOTSA_DELAY_ON_BOOT*1000);
  IFDEBUG IotsaSerial.printf("Delayed %d seconds on boot...\n", IOTSA_DELAY_ON_BOOT);
#endif
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

  // Ensure the CoAP/HPS companion modules exist before any module's setup() runs,
  // rather than being lazily created as a side effect of whichever module happens to
  // construct an IotsaApiServiceCoap/Hps member first (see cwi-dis/iotsa#113 case 3).
#ifdef IOTSA_WITH_COAP
  IotsaApiServiceCoap::ensureServiceMod(*this);
#endif
#ifdef IOTSA_WITH_HPS
  IotsaApiServiceHps::ensureServiceMod(*this);
#endif

  IotsaBaseModule *m;
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
  IotsaBaseModule *m;

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }

  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->lateSetupDone();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->lateSetupDone();
  }
}

void
IotsaApplication::loop() {
  iotsaConfig.loop();
  IotsaBaseModule *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->loop();
  }
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
String IotsaBaseModule::info() {
  // Info method that does nothing, usually overridden for IotsaBaseModule modules
  return "";
}

String IotsaBaseModule::htmlEncode(String data) {
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
void IotsaBaseModule::percentDecode(const String &src, String &dst) {
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

bool IotsaBaseModule::needsAuthentication(const char *object, IotsaApiOperation verb) { 
  return auth ? !auth->allows(object, verb) : false; 
}

bool IotsaBaseModule::needsAuthentication(const char *right) { 
  return auth ? !auth->allows(right) : false; 
}

void IotsaBaseModule::serverSetup() {
  // setup method that does nothing, usually overridden for IotsaBaseModule modules
}
