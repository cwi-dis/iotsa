#ifndef _IOTSASTANDARDMODULES_H_
#define _IOTSASTANDARDMODULES_H_
#include "iotsa.h"
#include "iotsaApi.h"

// Authenticator modules go first (so they are initialized when
// the later modules refer to them, through IotsaApplication::authenticatorPtr)
#ifdef IOTSA_WITH_USER
#include "iotsaUser.h"
IotsaUserMod userMod(application);
#endif

#ifdef IOTSA_WITH_WIFI
#include "iotsaWifi.h"
IotsaWifiMod wifiMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#ifdef IOTSA_WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
IotsaBLEServerMod bleserverMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#ifdef IOTSA_WITH_BATTERY
#include "iotsaBattery.h"
IotsaBatteryMod batteryMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif


#ifdef IOTSA_WITH_FILES
#include "iotsaFiles.h"
IotsaFilesMod filesMod(*IotsaApplication::applicationPtr);
#endif

#ifdef IOTSA_WITH_FILESUPLOAD
#include "iotsaFilesUpload.h"
IotsaFilesUploadMod filesUploadMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif


#ifdef IOTSA_WITH_FILESBACKUP
#include "iotsaFilesBackup.h"
IotsaFilesBackupMod filesBackupMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#ifdef IOTSA_WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#ifdef IOTSA_WITH_LOGGER
#include "iotsaLogger.h"
IotsaLoggerMod loggerMod(*IotsaApplication::applicationPtr, IotsaApplication::authenticatorPtr);
#endif

#endif