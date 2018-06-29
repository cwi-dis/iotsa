#ifndef _IOTSA_BUILDOPTIONS_H_
#define _IOTSA_BUILDOPTIONS_H_

#ifndef IOTSA_WITHOUT_DEBUG
#define IOTSA_WITH_DEBUG
#endif

#ifndef IOTSA_WITHOUT_WIFI
#define IOTSA_WITH_WIFI
#endif

#ifndef IOTSA_WITHOUT_HTTP
#define IOTSA_WITH_HTTP
#endif

#ifndef IOTSA_WITHOUT_HTTPS
// #define IOTSA_WITH_HTTPS
#endif

#if defined(IOTSA_WITH_HTTP) || defined(IOTSA_WITH_HTTPS)
#define IOTSA_WITH_HTTP_OR_HTTPS
#endif

#ifndef IOTSA_WITHOUT_WEB
#define IOTSA_WITH_WEB
#endif

#ifndef IOTSA_WITHOUT_API
#define IOTSA_WITH_API
#endif

#if !defined(IOTSA_WITHOUT_REST) && !defined(IOTSA_WITHOUT_API)
#define IOTSA_WITH_REST
#endif

#if !defined(IOTSA_WITHOUT_COAP) && !defined(IOTSA_WITHOUT_API)
// #define IOTSA_WITH_COAP
#endif

#ifndef IOTSA_WITHOUT_TIMEZONE_LIBRARY
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