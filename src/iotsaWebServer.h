#ifndef _IOTSAWEBSERVER_H_
#define _IOTSAWEBSERVER_H_
#include "iotsaBuildOptions.h"

//
// There are numerous different webserver implementations with very similar API.
// Attempt to select the correct one to use.
//
#if defined(ESP32) && defined(IOTSA_WITH_HTTP) && defined(IOTSA_WITH_STD_ESP32WEBSERVER)
#include <WebServer.h>
typedef WebServer IotsaWebServer;
typedef WebServer IotsaHttpWebServer;

#elif defined(ESP32) && defined(IOTSA_WITH_HTTPS)
#include <ESPWebServerSecure.hpp>
typedef ESPWebServerSecure IotsaWebServer;
typedef ESPWebServer IotsaHttpWebServer;
#define IOTSA_WEBSERVER_COMPAT

#elif defined(ESP32) && defined(IOTSA_WITH_HTTP)
#include <ESPWebServer.hpp>
typedef ESPWebServer IotsaWebServer;
typedef ESPWebServer IotsaHttpWebServer;
#define IOTSA_WEBSERVER_COMPAT

#elif !defined(ESP32) && defined(IOTSA_WITH_HTTPS)
#include <ESP8266WebServerSecure.h>
typedef BearSSL::ESP8266WebServerSecure IotsaWebServer;
typedef ESP8266WebServer IotsaHttpWebServer;

#elif !defined(ESP32) && defined(IOTSA_WITH_HTTP)
#include <ESP8266WebServer.h>
typedef ESP8266WebServer IotsaWebServer;
typedef ESP8266WebServer IotsaHttpWebServer;

#elif defined(IOTSA_HAS_WEBSERVER)
#error "Cannot determine WebServer implementation to use"
#endif

#ifdef IOTSA_HAS_WEBSERVER
// Call at the start of a request handler. The compat server (esp32_idf5_https_server)
// keeps a keep-alive connection, and on HTTPS its ~45 KB TLS session, until it
// times out (20 s) when it misses the client's close; on a memory-tight ESP32
// the next connection then can't get its session (cwi-dis/iotsa#293, a
// workaround for jackjansen/esp32_idf5_https_server#9). The other servers close
// after every response anyway.
inline void iotsaNoKeepAlive(IotsaWebServer *server) {
#ifdef IOTSA_WEBSERVER_COMPAT
  server->sendHeader("Connection", "close");
#endif
}
#endif

#endif
