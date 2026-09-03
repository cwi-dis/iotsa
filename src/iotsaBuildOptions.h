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
//
// This file is laid out in six stages, in order:
//   1. Plain-value defaults, independent of any WITH/WITHOUT flag.
//   2. Default-on WITH flags (each with a WITHOUT counterpart to opt out).
//   3. Default-off WITH flags, listed/documented (opt in with a bare -DIOTSA_WITH_xxx;
//      nothing here defines them).
//   4. Sanity checks on raw user input -- flag a WITH/WITHOUT combination that's
//      unsupported or needs a warning, before any derivation happens.
//   5. Derived (HAS_xxx) values, computed from stages 1-3's flags.
//   6. Sanity checks on derived values -- needs stage 5 to already be computed.
#define ESP_OPENSSL_SUPPRESS_LEGACY_WARNING

// ---- Stage 1: plain-value defaults ----

#ifndef IOTSA_SERIAL_SPEED
#define IOTSA_SERIAL_SPEED 115200
#endif

#ifndef IOTSA_LOGGER_BUFFER_SIZE
#define IOTSA_LOGGER_BUFFER_SIZE 4096
#endif

#ifndef IOTSA_WIFI_TIMEOUT
#define IOTSA_WIFI_TIMEOUT 30
#endif

// IOTSA_DELAY_ON_BOOT has no default and isn't defined here at all -- it's pure
// opt-in (a build config passes e.g. -DIOTSA_DELAY_ON_BOOT=3), tested with a bare
// #ifdef at its one use site (iotsa.cpp). Gives a native-USB board's host time to
// open the CDC port before the first boot prints happen.

// ---- Stage 2: default-on WITH flags ----

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

#ifndef IOTSA_WITHOUT_WEB
// web support (including uploads) is enabled by default
#define IOTSA_WITH_WEB
#endif

#ifndef IOTSA_WITHOUT_API
// Rest or Coap API is enabled by default
#define IOTSA_WITH_API
#endif

#ifndef IOTSA_WITHOUT_TIMEZONE
#define IOTSA_WITH_TIMEZONE
#endif

// ---- Stage 3: default-off WITH flags (opt in directly; nothing here defines them) ----

// https is NOT enabled by default. Opt in with -DIOTSA_WITH_HTTPS.
// #define IOTSA_WITH_HTTPS

// On esp32, use esp32_https_server_compat by default, unless IOTSA_WITH_STD_ESP32WEBSERVER
// is defined, then use the standard WebServer.h instead.
// #define IOTSA_WITH_STD_ESP32WEBSERVER

// Coap API is NOT enabled by default. Opt in with -DIOTSA_WITH_COAP.
// #define IOTSA_WITH_COAP

// BLE support is NOT enabled by default (ESP32-only). Opt in with -DIOTSA_WITH_BLE.
// #define IOTSA_WITH_BLE

// Sleep/wake support (the esp_*_sleep_start() machinery in IotsaRunmodeMod, driven
// by IotsaController's IotsaSleepPolicy). Not set here: opt in with
// -DIOTSA_WITH_SLEEP, opt out with -DIOTSA_WITHOUT_SLEEP. When neither is given it
// defaults on for BLE builds -- see IOTSA_HAS_SLEEP in stage 5. cwi-dis/iotsa#106.
// #define IOTSA_WITH_SLEEP

// Gives a module a lenient, no-op placeholder implementation instead of omitting it
// entirely when its real dependency (e.g. IOTSA_WITH_WEB) is off -- see
// IotsaFilesUploadMod/IotsaFilesBackupMod/IotsaFilesMod/IotsaWifiMod's own headers.
// #define IOTSA_WITH_PLACEHOLDERS

// IOTSA_WEBSERVER_PORT's default depends on stage 3's IOTSA_WITH_HTTPS, so it's set
// here rather than in stage 1.
#ifndef IOTSA_WEBSERVER_PORT
#ifdef IOTSA_WITH_HTTPS
#define IOTSA_WEBSERVER_PORT 443
#else
#define IOTSA_WEBSERVER_PORT 80
#endif
#endif

// ---- Stage 4: sanity checks on raw user input ----

#if defined(IOTSA_WITH_BLE) && !defined(ESP32)
#error IOTSA BLE support only available on ESP32
#endif

#if defined(IOTSA_WITH_HTTPS) && !defined(ESP32)
#warning IOTSA_WITH_HTTPS on ESP8266 is unreliable, see cwi-dis/iotsa#159
#endif

// ---- Stage 5: derived (HAS_xxx) values ----

#if defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS)
// We have some kind of HTTP(S) web server object at all.
#define IOTSA_HAS_WEBSERVER
#endif

#if defined(IOTSA_WITH_HTTP) && defined(IOTSA_WITH_HTTPS)
// Plain HTTP and HTTPS are both on: the tiny http->https redirect server exists
// alongside the real one.
#define IOTSA_HAS_FORWARDING_WEBSERVER
#endif

#if !defined(IOTSA_WITHOUT_REST) && !defined(IOTSA_WITHOUT_API)
// Rest API is enabled by default.
#define IOTSA_HAS_RESTSERVER
#endif

#if defined(IOTSA_WITH_COAP) && !defined(IOTSA_WITHOUT_API)
#define IOTSA_HAS_COAPSERVER
#endif

// If BLE support is enabled, HPS isn't individually disabled, and API support is
// enabled, we have an HPS service.
#if defined(IOTSA_WITH_BLE) && !defined(IOTSA_WITHOUT_HPS) && defined(IOTSA_WITH_API)
#define IOTSA_HAS_HPSSERVER
#endif

// Sleep/wake: explicit opt-in, or on-by-default for BLE builds (today's best
// proxy for an off-grid / battery device), unless explicitly opted out. Retune
// the derivation here if an IOTSA_WITH_BATTERY flag ever lands. cwi-dis/iotsa#106.
#if defined(IOTSA_WITH_SLEEP) || (defined(IOTSA_WITH_BLE) && !defined(IOTSA_WITHOUT_SLEEP))
#define IOTSA_HAS_SLEEP
#endif

// ---- Stage 6: sanity checks on derived values ----

#if defined(IOTSA_HAS_RESTSERVER) && !defined(IOTSA_HAS_WEBSERVER)
#error IOTSA REST support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_WEB) && !defined(IOTSA_HAS_WEBSERVER)
#error IOTSA WEB support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_API) && !(defined(IOTSA_HAS_RESTSERVER) || defined(IOTSA_HAS_COAPSERVER) || defined(IOTSA_HAS_HPSSERVER))
#error IOTSA API support requires REST, COAP or HPS
#endif

#endif
