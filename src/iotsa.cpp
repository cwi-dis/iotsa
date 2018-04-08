#include <Esp.h>
#include <FS.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include "iotsa.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;

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
  IFDEBUG Serial.begin(115200);
  IFDEBUG IotsaSerial.println("Serial opened");
  IFDEBUG IotsaSerial.print("Opening SPIFFS (may take long)...");
  bool ok = SPIFFS.begin();
  IFDEBUG IotsaSerial.println(" done.");
  if (!ok) {
    IFDEBUG IotsaSerial.println("SPIFFS.begin() failed, formatting");

#ifdef ESP32not
	// Create directories we know we need
	SPIFFS.mkdir("/config");
	SPIFFS.mkdir("/data");
#else
    ok = SPIFFS.format();
    if (!ok) {
      IFDEBUG IotsaSerial.println("SPIFFS.format() failed");
    }
    ok = SPIFFS.begin();
#endif
    if (!ok) {
      IFDEBUG IotsaSerial.println("SPIFFS.begin() after format failed");
    }
  } else {
    IFDEBUG IotsaSerial.println("SPIFFS mounted");
  }
  IotsaBaseMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->setup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->setup();
  }
}

void
IotsaApplication::serverSetup() {
  IotsaBaseMod *m;

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }

  server.onNotFound(std::bind(&IotsaApplication::webServerNotFoundHandler, this));
  server.on("/", std::bind(&IotsaApplication::webServerRootHandler, this));

  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
  webServerSetup();
}

void
IotsaApplication::loop() {
  IotsaBaseMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->loop();
  }
  webServerLoop();
}

void
IotsaApplication::webServerSetup() {
  server.begin();
  IFDEBUG IotsaSerial.println("HTTP server started");
}

void
IotsaApplication::webServerNotFoundHandler() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void
IotsaApplication::webServerRootHandler() {
  String message = "<html><head><title>" + title + "</title></head><body><h1>" + title + "</h1>";
  IotsaBaseMod *m;
  for (m=firstModule; m; m=m->nextModule) {
    message += m->info();
  }
  for (m=firstEarlyModule; m; m=m->nextModule) {
    message += m->info();
  }
  message += "</body></html>";
  server.send(200, "text/html", message);
}

void
IotsaApplication::webServerLoop() {
  server.handleClient();
}

bool IotsaBaseMod::needsAuthentication(const char *object, IotsaApiOperation verb) { 
  return auth ? !auth->allows(object, verb) : false; 
}

bool IotsaBaseMod::needsAuthentication(const char *right) { 
  return auth ? !auth->allows(right) : false; 
}

String IotsaBaseMod::info() {
  // Info method that does nothing, usually overridden for IotsaMod modules
  return "";
}

void IotsaBaseMod::serverSetup() {
  // setup method that does nothing, usually overridden for IotsaMod modules
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

