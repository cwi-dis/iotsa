#ifndef _IOTSA_BUILDOPTIONS_H_
#define _IOTSA_BUILDOPTIONS_H_

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

#if defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS)
#define IOTSA_WITH_HTTP_OR_HTTPS
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
// Rest API is enabled by default
#define IOTSA_WITH_REST
#endif

#if !defined(IOTSA_WITHOUT_COAP) && !defined(IOTSA_WITHOUT_API)
// Coap API is NOT enabled by default
// #define IOTSA_WITH_COAP
#endif

#ifndef IOTSA_WITHOUT_TIMEZONE_LIBRARY
// Timezone support is enabled in NTP module by default
#define IOTSA_WITH_TIMEZONE_LIBRARY
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

//#if defined(IOTSA_WITH_HTTP) && defined(IOTSA_WITH_HTTPS)
//#error IOTSA HTTP or HTTPS can be defined, not both
//#endif

#if defined(IOTSA_WITH_REST) && !(defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS))
#error IOTSA REST support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_WEB) && !(defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS))
#error IOTSA WEB support requires HTTP or HTTPS support
#endif

#if defined(IOTSA_WITH_API) && !(defined(IOTSA_WITH_REST) || defined(IOTSA_WITH_COAP))
#error IOTSA API support requires REST or COAP
#endif

#endif