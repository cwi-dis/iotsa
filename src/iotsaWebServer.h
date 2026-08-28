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

#elif defined(ESP32) && defined(IOTSA_WITH_HTTP)
#include <ESPWebServer.hpp>
typedef ESPWebServer IotsaWebServer;
typedef ESPWebServer IotsaHttpWebServer;

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

#endif
