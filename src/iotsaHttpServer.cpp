#include "iotsaHttpServer.h"

#ifdef IOTSA_HAS_FORWARDING_WEBSERVER
// Tiny http server which forwards to https
class TinyForwardServer {
public:
  IotsaHttpWebServer server;
  TinyForwardServer()
  : server(80)
  {
    server.onNotFound(std::bind(&TinyForwardServer::notFound, this));
    server.begin();
  }
  void notFound() {
    String newLoc = "https://";
    if (!iotsaConfig.mdnsEnabled) {
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

#endif // defined(IOTSA_HAS_FORWARDING_WEBSERVER)

IotsaHttpServiceMod::IotsaHttpServiceMod(IotsaApplication &_app)
: IotsaBaseModule(_app, nullptr, true)
{
  claimSingleton(this);
#ifdef IOTSA_HAS_WEBSERVER
  server = new IotsaWebServer(IOTSA_WEBSERVER_PORT);
#endif
}

void
IotsaHttpServiceMod::setup() {
  name = "http";
}

#ifdef IOTSA_HAS_WEBSERVER
void
IotsaHttpServiceMod::lateSetup() {
  if (!iotsaConfig.wifiEnabled) return;

#ifdef IOTSA_HAS_FORWARDING_WEBSERVER
  if (singletonTFS == NULL)
    singletonTFS = new TinyForwardServer();
#endif // defined(IOTSA_HAS_FORWARDING_WEBSERVER)

  server->onNotFound(std::bind(&IotsaHttpServiceMod::webServerNotFoundHandler, this));
#ifdef IOTSA_WITH_WEB
  server->on("/", std::bind(&IotsaHttpServiceMod::webServerRootHandler, this));
#endif

#ifdef IOTSA_WITH_HTTPS
  IFDEBUG IotsaSerial.print("Using https key len=");
  IFDEBUG IotsaSerial.print(iotsaConfig.httpsKeyLength);
  IFDEBUG IotsaSerial.print(", cert len=");
  IFDEBUG IotsaSerial.println(iotsaConfig.httpsCertificateLength);
#if !defined(ESP32) && defined(IOTSA_WITH_HTTPS)
  // BearSSL's ESP8266WebServerSecure is the only implementation exposing setRSACert()
  // -- everyone else (both ESP32 implementations, and plain non-HTTPS ESP8266) uses
  // setServerKeyAndCert_P() instead.
  X509List *chain = new X509List(iotsaConfig.httpsCertificate, iotsaConfig.httpsCertificateLength);
  PrivateKey *sk = new PrivateKey(iotsaConfig.httpsKey, iotsaConfig.httpsKeyLength);
  if (!chain || !sk) {
    IotsaSerial.print("ssl: out of memory");
  } else {
    server->getServer().setRSACert(chain, sk);
  }
#else
  server->getServer().setServerKeyAndCert_P(
    iotsaConfig.httpsKey,
    iotsaConfig.httpsKeyLength,
    iotsaConfig.httpsCertificate,
    iotsaConfig.httpsCertificateLength
  );
#endif
#endif
  server->begin();
  serverInitialized = true;
  IFDEBUG IotsaSerial.println("Web server started");
}

void
IotsaHttpServiceMod::loop() {
  if (!iotsaConfig.wifiEnabled) return;
  if (!serverInitialized) {
    // Wifi is enabled but the server has not been initialized yet.
    // Apparently wifi was disabled when we booted, so setup the server
    // now.
    IFDEBUG IotsaSerial.println("Setup web server after WiFi enabled");
    lateSetup();
    return;
  }
  server->handleClient();
#ifdef IOTSA_HAS_FORWARDING_WEBSERVER
  singletonTFS->server.handleClient();
#endif
}

void
IotsaHttpServiceMod::webServerNotFoundHandler() {
  iotsaConfig.postponeSleep(0);
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
#else // IOTSA_HAS_WEBSERVER
void IotsaHttpServiceMod::lateSetup() {}
void IotsaHttpServiceMod::loop() {}
#endif // IOTSA_HAS_WEBSERVER

#ifdef IOTSA_WITH_WEB
void
IotsaHttpServiceMod::webServerRootHandler() {
  iotsaConfig.postponeSleep(0);
  String message = "<html><head><title>" + app.title + "</title></head><body><h1>" + app.title + "</h1>";
  IotsaBaseModule *m;
  for (m=app.firstModule; m; m=m->nextModule) {
    message += m->info();
  }
  for (m=app.firstEarlyModule; m; m=m->nextModule) {
    message += m->info();
  }
  message += "</body></html>";
  server->send(200, "text/html", message);
}
#endif // IOTSA_WITH_WEB
