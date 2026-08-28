#ifndef _IOTSA_BUILDOPTIONS_H_
#define _IOTSA_BUILDOPTIONS_H_

// Naming contract for this file's two families of feature macros (see cwi-dis/iotsa#205):
//
// - IOTSA_WITH_xxx / IOTSA_WITHOUT_xxx are the only macros a build config (platformio.ini,
//   iotsa-build.json, an app's own build_flags) ever sets directly -- either opting out of
//   a default-on feature (IOTSA_WITHOUT_xxx) or opting into a default-off one (a bare
//   IOTSA_WITH_xxx, e.g. IOTSA_WITH_BLE/HTTPS/COAP). This file is the only place that
//   #defines an IOTSA_WITH_xxx; nowhere else in iotsa should one be #define'd.
// - IOTSA_HAS_xxx is a fact computed here from other flags (a straight passthrough, an
//   AND/OR of several, or both) -- never set directly by a build config, only ever tested
//   with #ifdef elsewhere. If you're tempted to #define a new IOTSA_WITH_xxx outside this
//   file, or to derive one flag from others under a WITH_ name, it should be a HAS_ instead.
#define ESP_OPENSSL_SUPPRESS_LEGACY_WARNING

#ifndef IOTSA_SERIAL_SPEED
#define IOTSA_SERIAL_SPEED 115200
#endif

#ifndef IOTSA_WITHOUT_DEBUG
// Debug prints are enabled by default
#define IOTSA_WITH_DEBUG
#endif

#ifndef IOTSA_WITHOUT_WIFI
// WiFi is enabled by default
#define IOTSA_WITH_WIFI
#endif

#ifndef IOTSA_WITHOUT_HTTP
// http is enabled by default
#define IOTSA_WITH_HTTP
#endif

#ifndef IOTSA_WITHOUT_HTTPS
// https is NOT enabled by default
// #define IOTSA_WITH_HTTPS
#endif

// On esp32, use esp32_https_server_compat by default, 
// unless IOTSA_WITH_STD_ESP32WEBSERVER is defined, then use the standard WebServer.h
// #define IOTSA_WITH_STD_ESP32WEBSERVER

#if defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS)
// We have some kind of HTTP(S) web server object at all -- see cwi-dis/iotsa#205.
#define IOTSA_HAS_WEBSERVER
#endif

#if defined(IOTSA_WITH_HTTP) && defined(IOTSA_WITH_HTTPS)
// Plain HTTP and HTTPS are both on: the tiny http->https redirect server exists
// alongside the real one -- see cwi-dis/iotsa#205.
#define IOTSA_HAS_FORWARDING_WEBSERVER
#endif

#ifndef IOTSA_WITHOUT_WEB
// web support (including uploads) is enabled by default
#define IOTSA_WITH_WEB
#endif

#ifndef IOTSA_WITHOUT_API
// Rest or Coap API is enabled by default
#define IOTSA_WITH_API
#endif

#if !defined(IOTSA_WITHOUT_REST) && !defined(IOTSA_WITHOUT_API)
// Rest API is enabled by default. IOTSA_WITH_REST is never set directly by a build
// config (only IOTSA_WITHOUT_REST/IOTSA_WITHOUT_API are), so it's purely an internal
// derivation step here -- the rest of iotsa tests IOTSA_HAS_RESTSERVER instead.
#define IOTSA_HAS_RESTSERVER
#endif

#if !defined(IOTSA_WITHOUT_COAP) && !defined(IOTSA_WITHOUT_API)
// Coap API is NOT enabled by default -- opt in with IOTSA_WITH_COAP.
// xxxjack this condition (and IOTSA_WITHOUT_API's role in it) is currently vestigial:
// IOTSA_WITH_COAP is only ever turned on by a build config passing it directly, and
// nothing here re-disables it when IOTSA_WITHOUT_API is also set. Parked for the
// iotsaBuildOptions.h rationalization pass, see cwi-dis/iotsa#205.
#endif
#ifdef IOTSA_WITH_COAP
#define IOTSA_HAS_COAPSERVER
#endif

// BLE support is NOT enabled by default on ESP32
//#ifdef ESP32
//#define IOTSA_WITH_BLE
//#endif
// If BLE support is enabled and API support is enabled we enable HPS service by default.
// IOTSA_WITH_HPS is never set directly by a build config (only IOTSA_WITHOUT_HPS is),
// so -- like IOTSA_WITH_REST above -- it's purely an internal derivation step; the rest
// of iotsa tests IOTSA_HAS_HPSSERVER instead.
// xxxjack this condition doesn't actually check IOTSA_WITH_API, despite the comment
// above -- and once BLE-client-only builds are real (cwi-dis/iotsa#84), "BLE is on"
// won't even imply a BLE *server* exists, which HPS requires. Parked for the
// iotsaBuildOptions.h rationalization pass, see cwi-dis/iotsa#205.
#ifdef IOTSA_WITH_BLE
#ifndef IOTSA_WITHOUT_HPS
#define IOTSA_HAS_HPSSERVER
#endif
#endif

#ifndef IOTSA_WITHOUT_TIMEZONE
#define IOTSA_WITH_TIMEZONE
#endif

// #define IOTSA_WITH_PLACEHOLDERS

#ifndef IOTSA_WEBSERVER_PORT
#ifdef IOTSA_WITH_HTTPS
#define IOTSA_WEBSERVER_PORT 443
#else
#define IOTSA_WEBSERVER_PORT 80
#endif
#endif

#ifndef IOTSA_LOGGER_BUFFER_SIZE
#define IOTSA_LOGGER_BUFFER_SIZE 4096
#endif

#ifndef IOTSA_WIFI_TIMEOUT
#define IOTSA_WIFI_TIMEOUT 30
#endif

// Consistency checks

#if defined(IOTSA_HAS_RESTSERVER) && !defined(IOTSA_HAS_WEBSERVER)
#error IOTSA REST support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_WEB) && !defined(IOTSA_HAS_WEBSERVER)
#error IOTSA WEB support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_API) && !(defined(IOTSA_HAS_RESTSERVER) || defined(IOTSA_HAS_COAPSERVER) || defined(IOTSA_HAS_HPSSERVER))
#error IOTSA API support requires REST, COAP or HPS
#endif

#if defined(IOTSA_WITH_BLE) && !defined(ESP32)
#error IOTSA BLE support only available on ESP32
#endif

#if defined(IOTSA_WITH_HTTPS) && !defined(ESP32)
#warning IOTSA_WITH_HTTPS on ESP8266 is unreliable, see cwi-dis/iotsa#159
#endif
#endif