#ifndef _IOTSAWIFIDEBUG_H_
#define _IOTSAWIFIDEBUG_H_
#include "iotsa.h"   // IotsaSerial

// Shared wifi-module logging (cwi-dis/iotsa#176 print rationalization). Not a
// public header -- included directly by iotsaWifi.cpp/iotsaWifiController.cpp
// only, so these names don't leak into every translation unit that pulls in
// the public iotsaWifi.h.
//
// WCLOG -- always on, one "iotsaWifi: "-prefixed printf call (no trailing \n
// needed in the format string). For connectivity facts a human always wants
// to see: connected/lost, AP up, a new STA attempt starting.
//
// WCDEBUG -- same, but only when IOTSA_WIFI_DEBUG is defined (opt-in). For
// internal state-machine chatter: hunt/AP window transitions, reconcile
// decisions, staState() transitions, etc.
#define WCLOG(fmt, ...) do { IotsaSerial.printf("iotsaWifi: " fmt "\n", ##__VA_ARGS__); } while (0)
#ifdef IOTSA_WIFI_DEBUG
#define WCDEBUG(fmt, ...) WCLOG(fmt, ##__VA_ARGS__)
#else
#define WCDEBUG(...) do {} while (0)
#endif

#endif
