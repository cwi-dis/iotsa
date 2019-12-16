#include "iotsa.h"
#if 0

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

IotsaWebServerMixin::IotsaWebServerMixin(IotsaApplication* _app)
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
:
  server(new IotsaWebServer(IOTSA_WEBSERVER_PORT)),
  app(_app)
#endif
{
}

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
void
IotsaWebServerMixin::webServerSetup() {
  if (!iotsaConfig.wifiEnabled) return;

#if defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
  if (singletonTFS == NULL)
    singletonTFS = new TinyForwardServer();
#endif // defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  server->onNotFound(std::bind(&IotsaWebServerMixin::webServerNotFoundHandler, this));
#endif
#ifdef IOTSA_WITH_WEB
  server->on("/", std::bind(&IotsaWebServerMixin::webServerRootHandler, this));
#endif

#ifdef IOTSA_WITH_HTTPS
  IFDEBUG IotsaSerial.print("Using https key len=");
  IFDEBUG IotsaSerial.print(iotsaConfig.httpsKeyLength);
  IFDEBUG IotsaSerial.print(", cert len=");
  IFDEBUG IotsaSerial.println(iotsaConfig.httpsCertificateLength);
  server->getServer().setServerKeyAndCert_P(
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
IotsaWebServerMixin::webServerLoop() {
  if (!iotsaConfig.wifiEnabled) return;
  server->handleClient();
#if defined(IOTSA_WITH_HTTPS) && defined(IOTSA_WITH_HTTP)
  singletonTFS->server.handleClient();
#endif
}

void
IotsaWebServerMixin::webServerNotFoundHandler() {
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
IotsaWebServerMixin::webServerRootHandler() {
  String message = "<html><head><title>" + app->title + "</title></head><body><h1>" + app->title + "</h1>";
  IotsaBaseMod *m;
  for (m=app->firstModule; m; m=m->nextModule) {
    message += m->info();
  }
  for (m=app->firstEarlyModule; m; m=m->nextModule) {
    message += m->info();
  }
  message += "</body></html>";
  server->send(200, "text/html", message);
}
#endif // IOTSA_WITH_WEB
#endif