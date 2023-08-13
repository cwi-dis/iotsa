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

//#define IOTSA_WITH_USER   // Enable username/password authentication for changing configurations
#define IOTSA_WITH_NTP    // Use network time protocol to synchronize the clock.
#define IOTSA_WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
//#define IOTSA_WITH_FILES  // Enable static files webserver
//#define IOTSA_WITH_FILESUPLOAD  // Enable upload of static files for webserver
//#define IOTSA_WITH_FILESBACKUP  // Enable backup of all files including config files and webserver files
#define IOTSA_WITH_BATTERY // Enable power-saving support

IotsaApplication application("Iotsa Skeleton Server");

#include "iotsaStandardModules.h"

IotsaWifiMod wifiMod(application);

#ifdef IOTSA_WITH_BATTERY
#define PIN_DISABLE_SLEEP 0 // Define for pin on which low signal disables sleep
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

