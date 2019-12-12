#include <Esp.h>
#include <FS.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include "iotsa.h"
#include "iotsaConfig.h"

// There is an issue with the platformio library dependency finder, and it doesn't find the
// esp8266httpclient library. This is a workaround.
#include "iotsaRequest.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;

#if defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
// Tiny http server which forwards to https
class TinyForwardServer {
public:
  ESP8266WebServer server;
  TinyForwardServer()
  : server(80)
  {
    server.onNotFound(std::bind(&TinyForwardServer::notFound, this));
    server.begin();
  }
  void notFound() {
    String newLoc = "https://";
    if (iotsaConfig.wifiPrivateNetworkMode) {
      newLoc += "192.168.4.1";
    } else {
      newLoc += iotsaConfig.hostName;
      newLoc += ".local";
    }
    newLoc += server.uri();
    IFDEBUG IotsaSerial.print("HTTP 301 to ");
    IFDEBUG IotsaSerial.println(newLoc);
    server.sendHeader("Location", newLoc);
    server.uri();
    server.send(301, "", "");
  }
};

static TinyForwardServer *singletonTFS;

#endif // defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)

IotsaApplication::IotsaApplication(const char *_title)
: status(NULL),
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  server(new IotsaWebServer(IOTSA_WEBSERVER_PORT)),
#endif
  firstModule(NULL), 
  firstEarlyModule(NULL), 
  title(_title),
  haveOTA(false)
{
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
  // Normally iotsaConfigMod is initialized by the WiFi module,
  // but if the WiFi module isn't indluded we ensure that there is a config module anyway.
  if (IotsaConfigMod::singleton == NULL) {
    (void)new IotsaConfigMod(*this);
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
#if defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
  if (singletonTFS == NULL)
    singletonTFS = new TinyForwardServer();
#endif // defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
  IFDEBUG IotsaSerial.print("hostname: ");
  IFDEBUG IotsaSerial.println(iotsaConfig.hostName);
}

void
IotsaApplication::serverSetup() {
  if (!iotsaConfig.wifiEnabled) return;
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
  if (!iotsaConfig.wifiEnabled) return;
#ifdef IOTSA_WITH_HTTPS
  IFDEBUG IotsaSerial.print("Using https key len=");
  IFDEBUG IotsaSerial.print(iotsaConfig.httpsKeyLength);
  IFDEBUG IotsaSerial.print(", cert len=");
  IFDEBUG IotsaSerial.println(iotsaConfig.httpsCertificateLength);
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
  if (!iotsaConfig.wifiEnabled) return;
  server->handleClient();
#if defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
  singletonTFS->server.handleClient();
#endif
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
