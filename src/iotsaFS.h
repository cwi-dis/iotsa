#ifndef _IOTSAFS_H_
#define _IOTSAFS_H_
//
// SPIFFS/LittleFS choice is complex, also for include file differences on ESP32/ESP8266.
// So put if all in a separate include file.
//
#include <FS.h>

#ifdef IOTSA_WITH_LEGACY_SPIFFS
// Use SPIFFS. On ESP32 it requires an extra include, on 8266 not.
#ifdef ESP32
#include <SPIFFS.h>
#endif
#define IOTSA_FS SPIFFS
#define IOTSA_FS_NAME "SPIFFS"
#else
#if defined(ESP32) && !defined(PLATFORMIO)
// Use Lorol LittleFS when building for Arduino. It uses slightly different include file name.
#include <LITTLEFS.h>
#define IOTSA_FS LITTLEFS
#else
// Use normal (as of 2022) LittleFS on esp32 or esp8266.
#include <LittleFS.h>
#define IOTSA_FS LittleFS
#if defined(ESP32)
#define IOTSA_FS_OPEN_3ARGS // Open has a third create parameter
#endif
#endif
#define IOTSA_FS_NAME "LittleFS"
#endif

#endif
