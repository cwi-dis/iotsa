#include <Esp.h>
#include <FS.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include "iotsa.h"

// There is an issue with the platformio library dependency finder, and it doesn't find the
// esp8266httpclient library. This is a workaround.
#include "iotsaRequest.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;

IotsaApplication::IotsaApplication(const char *_title)
: status(NULL),
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  server(new IotsaWebServer(IOTSA_WEBSERVER_PORT)),
#endif
  firstModule(NULL), 
  firstEarlyModule(NULL), 
  title(_title),
  haveOTA(false)
{}

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

    ok = SPIFFS.format();
    if (!ok) {
      IFDEBUG IotsaSerial.println("SPIFFS.format() failed");
    }
    ok = SPIFFS.begin();
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
#ifndef ESP32
  ESP.wdtEnable(WDTO_120MS);
#endif
}

void
IotsaApplication::serverSetup() {
  IotsaBaseMod *m;

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  server->onNotFound(std::bind(&IotsaApplication::webServerNotFoundHandler, this));
#endif
#ifdef IOTSA_WITH_WEB
  server->on("/", std::bind(&IotsaApplication::webServerRootHandler, this));
#endif

  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  webServerSetup();
#endif
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
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  webServerLoop();
#endif
}

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
void
IotsaApplication::webServerSetup() {
#ifdef IOTSA_WITH_HTTPS
  server->setServerKeyAndCert_P(
    iotsaConfig.httpsKey,
    iotsaConfig.httpsKeyLength,
    iotsaConfig.httpsCertificate,
    iotsaConfig.httpsCertificateLength
  );
#endif
  server->begin();
#ifdef IOTSA_WITH_HTTPS
  IFDEBUG IotsaSerial.println("HTTPS server started");
#else
  IFDEBUG IotsaSerial.println("HTTP server started");
#endif
}

void
IotsaApplication::webServerLoop() {
  server->handleClient();
}

void
IotsaApplication::webServerNotFoundHandler() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i=0; i<server->args(); i++){
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}
#endif // IOTSA_WITH_HTTP_OR_HTTPS

#ifdef IOTSA_WITH_WEB
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
  server->send(200, "text/html", message);
}

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
