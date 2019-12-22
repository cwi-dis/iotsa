#ifndef _IOTSAWEBSERVER_H_
#define _IOTSAWEBSERVER_H_
#include "iotsaBuildOptions.h"

#if defined(IOTSA_WITH_HTTP) && !defined(IOTSA_WITH_HTTPS)
#ifdef ESP32
#ifdef IOTSA_WITH_ESP32HTTPSCOMPAT
#include <ESPWebServer.hpp>
typedef ESPWebServer IotsaWebServer;
#elif 1
#include <WebServer.h>
typedef WebServer IotsaWebServer;
#else
#include <ESP32WebServer.h>
typedef ESP32WebServer IotsaWebServer;
#endif
#else
#include <ESP8266WebServer.h>
typedef ESP8266WebServer IotsaWebServer;
#endif
#endif
#ifdef IOTSA_WITH_HTTPS
#ifdef ESP32
#error IOTSA_WITH_HTTPS is not supported for ESP32
#undef IOTSA_WITH_HTTPS
#define IOTSA_WITH_HTTP
#if 1
#include <WebServer.h>
typedef WebServer IotsaWebServer;
#else
// Older release of esp32 webserver, but unsure how to test for that
#include <ESP32WebServer.h>
typedef ESP32WebServer IotsaWebServer;
#endif
#else
#include <ESP8266WebServerSecure.h>
typedef axTLS::ESP8266WebServerSecure IotsaWebServer;
#endif
#endif

class IotsaApplication;

class IotsaWebServerMixin {
friend class IotsaApplication;
public:
  IotsaWebServerMixin(IotsaApplication* _app);
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
protected:
  IotsaApplication* app;
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
};

#endif
