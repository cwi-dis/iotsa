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
    // Redirect to whatever name or address the client used to reach us (works
    // for hostname.local, a bare IP, and the config AP's 192.168.4.1 alike),
    // minus any ":port". hostname.local only if the client sent no Host.
    String host = server.hostHeader();
    int colon = host.lastIndexOf(':');
    if (colon > host.lastIndexOf(']')) host = host.substring(0, colon);
    if (host.length() == 0) host = iotsaConfig.hostName + ".local";
    String newLoc = "https://" + host;
    newLoc += server.uri();
    // The web server only hands us the decoded arguments, not the raw query
    // string, so rebuild it (cwi-dis/iotsa#49). "plain" is the request body,
    // not a query argument.
    char sep = '?';
    for (int i = 0; i < server.args(); i++) {
      if (server.argName(i) == "plain") continue;
      newLoc += sep;
      newLoc += urlEncode(server.argName(i));
      newLoc += '=';
      newLoc += urlEncode(server.arg(i));
      sep = '&';
    }
    IFDEBUG IotsaSerial.print("HTTP 301 to ");
    IFDEBUG IotsaSerial.println(newLoc);
    server.sendHeader("Location", newLoc);
    server.send(301, "", "");
  }

  static String urlEncode(const String& in) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    for (size_t i = 0; i < in.length(); i++) {
      char c = in[i];
      if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
        out += c;
      } else {
        out += '%';
        out += hex[((unsigned char)c) >> 4];
        out += hex[((unsigned char)c) & 0xf];
      }
    }
    return out;
  }
};

static TinyForwardServer *singletonTFS;

#endif // defined(IOTSA_HAS_FORWARDING_WEBSERVER)

IotsaHttpServiceMod::IotsaHttpServiceMod(IotsaApplication &_app)
: IotsaBaseModule(_app, true)
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
  // Registering handlers needs no network; only begin() does.
  server->onNotFound(std::bind(&IotsaHttpServiceMod::webServerNotFoundHandler, this));
#ifdef IOTSA_WITH_WEB
  server->on("/", std::bind(&IotsaHttpServiceMod::webServerRootHandler, this));
#endif
  _startIfReady();
}

// Starts the server(s) once the TCP/IP stack is up: binding a socket before
// that crashes lwIP ("Invalid mbox", cwi-dis/iotsa#106). Called every loop(),
// so WiFi coming up late (disabled at boot, enabled at runtime) is fine too.
// Never stops: a server listening on all addresses survives the station
// dropping and the AP coming and going (cwi-dis/iotsa#239).
void
IotsaHttpServiceMod::_startIfReady() {
  if (_started || !iotsaStatus.networkStackUp) return;

#ifdef IOTSA_HAS_FORWARDING_WEBSERVER
  if (singletonTFS == NULL)
    singletonTFS = new TinyForwardServer();
#endif // defined(IOTSA_HAS_FORWARDING_WEBSERVER)

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
  _started = true;
  IFDEBUG IotsaSerial.println("Web server started");
}

void
IotsaHttpServiceMod::loop() {
  _startIfReady();
  if (!_started) return;
  server->handleClient();
#ifdef IOTSA_HAS_FORWARDING_WEBSERVER
  singletonTFS->server.handleClient();
#endif
}

void
IotsaHttpServiceMod::webServerNotFoundHandler() {
  iotsaController.noteActivity();
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
  iotsaController.noteActivity();
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
