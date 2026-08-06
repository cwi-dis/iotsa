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

// Use normal (as of 2022) LittleFS on esp32 or esp8266.
#include <LittleFS.h>
#define IOTSA_FS LittleFS
#define IOTSA_FS_NAME "LittleFS"

#endif

#ifndef ESP32
// Finally, on esp8266 open() does not have the third "create" argument.
#define IOTSA_FS_OPEN_2_ARGS
#endif

// Filesystem usage. On esp32 IOTSA_FS is a concrete FS subclass with totalBytes()/usedBytes(),
// on esp8266 it's a generic fs::FS and usage must be queried through info(FSInfo&) instead.
inline size_t iotsaFSTotalBytes() {
#ifdef ESP32
  return IOTSA_FS.totalBytes();
#else
  FSInfo info;
  IOTSA_FS.info(info);
  return info.totalBytes;
#endif
}

inline size_t iotsaFSUsedBytes() {
#ifdef ESP32
  return IOTSA_FS.usedBytes();
#else
  FSInfo info;
  IOTSA_FS.info(info);
  return info.usedBytes;
#endif
}

#endif
