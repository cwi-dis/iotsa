#ifndef _IOTSALOG_H_
#define _IOTSALOG_H_
#include <Print.h>

// Magic to allow logging to be kept in-core, if wanted, by using
// IotsaSerial in stead of Serial.
extern Print *iotsaOverrideSerial;
#define IotsaSerial (*iotsaOverrideSerial)

//
// Log and debug output (cwi-dis/iotsa#182), included by iotsa.h. All three macros take
// (tag, fmt, ...) and produce one IotsaSerial.printf() line "tag: ...\n":
//
// IOTSA_LOG               -- always on (off only with -DIOTSA_WITHOUT_LOG). For
//                            facts a person wants to see: state changes, errors,
//                            refused requests, a server starting.
// IOTSA_LOG_DEBUG         -- step-by-step chatter. Compiled in by default
//                            (IOTSA_WITH_DEBUG; -DIOTSA_WITHOUT_DEBUG removes it),
//                            and switchable per device at runtime through
//                            iotsaLogDebugEnabled (the "debugLog" field on
//                            /api/runmode, saved by IotsaRunmodeMod; true until
//                            that config has been read, so boot always logs).
// IOTSA_LOG_DEBUG_<X>     -- only with -DIOTSA_DEBUG_<X>, for subsystem X. Not
//                            affected by IOTSA_WITH_DEBUG or the runtime switch.
//                            The list below is the registry of all subsystems.
//
// tag and fmt must be string literals: they are glued together at compile time,
// and no trailing "\n" is needed. iotsa's own tags start with "iotsa" and are the
// module's file name ("iotsaWifi", "iotsaRunmode"), optionally with a sub-level
// ("iotsaApi: rest"); applications use their own name.
//
// A macro that is off still compiles its printf() inside "if (0)": the compiler
// checks format and arguments, but the arguments are never evaluated and the
// code and strings are removed.
//

#define _IOTSA_LOG_ON(tag, fmt, ...) do { IotsaSerial.printf(tag ": " fmt "\n", ##__VA_ARGS__); } while (0)
#define _IOTSA_LOG_OFF(tag, fmt, ...) do { if (0) IotsaSerial.printf(tag ": " fmt "\n", ##__VA_ARGS__); } while (0)

#ifndef IOTSA_WITHOUT_LOG
#define IOTSA_LOG(tag, fmt, ...) _IOTSA_LOG_ON(tag, fmt, ##__VA_ARGS__)
#else
#define IOTSA_LOG(tag, fmt, ...) _IOTSA_LOG_OFF(tag, fmt, ##__VA_ARGS__)
#endif

extern bool iotsaLogDebugEnabled;
#ifdef IOTSA_WITH_DEBUG
#define IOTSA_LOG_DEBUG(tag, fmt, ...) do { if (iotsaLogDebugEnabled) IOTSA_LOG(tag, fmt, ##__VA_ARGS__); } while (0)
#else
#define IOTSA_LOG_DEBUG(tag, fmt, ...) _IOTSA_LOG_OFF(tag, fmt, ##__VA_ARGS__)
#endif

// Subsystems.

#ifdef IOTSA_DEBUG_WIFI
#define IOTSA_LOG_DEBUG_WIFI(tag, fmt, ...) IOTSA_LOG(tag, fmt, ##__VA_ARGS__)
#else
#define IOTSA_LOG_DEBUG_WIFI(tag, fmt, ...) _IOTSA_LOG_OFF(tag, fmt, ##__VA_ARGS__)
#endif

#ifdef IOTSA_DEBUG_BLE
#define IOTSA_LOG_DEBUG_BLE(tag, fmt, ...) IOTSA_LOG(tag, fmt, ##__VA_ARGS__)
#else
#define IOTSA_LOG_DEBUG_BLE(tag, fmt, ...) _IOTSA_LOG_OFF(tag, fmt, ##__VA_ARGS__)
#endif

#ifdef IOTSA_DEBUG_INPUT
#define IOTSA_LOG_DEBUG_INPUT(tag, fmt, ...) IOTSA_LOG(tag, fmt, ##__VA_ARGS__)
#else
#define IOTSA_LOG_DEBUG_INPUT(tag, fmt, ...) _IOTSA_LOG_OFF(tag, fmt, ##__VA_ARGS__)
#endif

#endif
