#ifndef _IOTSAFS_H_
#define _IOTSAFS_H_
//
// SPIFFS/LittleFS choice is complex, also for include file differences on ESP32/ESP8266.
// So put if all in a separate include file.
//
#include <FS.h>

#ifdef IOTSA_WITH_LEGACY_SPIFFS
#ifdef ESP32
#include <SPIFFS.h>
#endif
#define IOTSA_FS SPIFFS
#define IOTSA_FS_NAME "SPIFFS"
#else
#ifdef ESP32
#include <LITTLEFS.h>
#define IOTSA_FS LITTLEFS
#else
#include <LittleFS.h>
#define IOTSA_FS LittleFS
#endif
#define IOTSA_FS_NAME "LittleFS"
#endif

#endif
