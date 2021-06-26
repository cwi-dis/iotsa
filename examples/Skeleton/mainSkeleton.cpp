//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// The server always includes the Wifi configuration module. You can enable
// other modules with the preprocessor defines. With the default defines the server
// will allow serving of web pages and other documents, and of uploading those.
//

#include <Arduino.h>
#include "iotsa.h"
#include "iotsaWifi.h"

// CHANGE: Add application includes and declarations here

#undef WITH_USER   // Enable username/password authentication for changing configurations
#define WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#undef WITH_FILES  // Enable static files webserver
#undef WITH_FILESUPLOAD  // Enable upload of static files for webserver
#undef WITH_FILESBACKUP  // Enable backup of all files including config files and webserver files
#define WITH_BATTERY // Enable power-saving support

IotsaApplication application("Iotsa Skeleton Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_USER
#include "iotsaUser.h"
IotsaUserMod userMod(application);
#define authProvider &userMod
#else
#define authProvider NULL
#endif

#ifdef WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application, authProvider);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, authProvider);
#endif

#ifdef WITH_FILES
#include "iotsaFiles.h"
IotsaFilesMod filesMod(application);
#endif

#ifdef WITH_FILESUPLOAD
#include "iotsaFilesUpload.h"
IotsaFilesUploadMod filesUploadMod(application, authProvider);
#endif

#ifdef WITH_FILESBACKUP
#include "iotsaFilesBackup.h"
IotsaFilesBackupMod filesBackupMod(application, authProvider);
#endif

#ifdef WITH_BATTERY
#define PIN_DISABLE_SLEEP 0 // Define for pin on which low signal disables sleep
#include "iotsaBattery.h"
IotsaBatteryMod batteryMod(application, authProvider);
#endif

void setup(void){
  application.setup();
  application.serverSetup();
#ifdef PIN_DISABLE_SLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLE_SLEEP);
#endif
}
 
void loop(void){
  application.loop();
}

