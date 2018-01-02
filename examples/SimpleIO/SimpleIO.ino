//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// The server always includes the Wifi configuration module. You can enable
// other modules with the preprocessor defines. With the default defines the server
// will allow serving of web pages and other documents, and of uploading those.
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaSimpleIO.h"

// CHANGE: Add application includes and declarations here

#undef WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_FILES  // Enable static files webserver
#define WITH_FILESUPLOAD  // Enable upload of static files for webserver
#define WITH_FILESBACKUP  // Enable backup of all files including config files and webserver files

IotsaWebServer server(80);
IotsaApplication application(server, "Iotsa Simple IO Server");
IotsaWifiMod wifiMod(application);

IotsaSimpleIOMod simpleIOMod(application);

#ifdef WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#ifdef WITH_FILES
#include "iotsaFiles.h"
IotsaFilesMod filesMod(application);
#endif

#ifdef WITH_FILESUPLOAD
#include "iotsaFilesUpload.h"
IotsaFilesUploadMod filesUploadMod(application);
#endif

#ifdef WITH_FILESBACKUP
#include "iotsaFilesBackup.h"
IotsaFilesBackupMod filesBackupMod(application);
#endif

void setup(void){
  application.setup();
  application.serverSetup();
#ifndef ESP32
  ESP.wdtEnable(WDTO_120MS);
#endif
#ifdef ESP32
  wifi_ps_type_t curMode;
  esp_wifi_get_ps(&curMode);
  IotsaSerial.print("esp_wifi_get_ps()=");
  IotsaSerial.println((int)curMode);
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  esp_wifi_set_ps(WIFI_PS_MODEM);
#endif
}
 
void loop(void){
  application.loop();
}

