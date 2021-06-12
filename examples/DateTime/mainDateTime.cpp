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

#define WITH_RTC    // Enable Realtime Clock support
#undef WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_BATTERY // Enable power-saving support

IotsaApplication application("Iotsa DateTime Server");
IotsaWifiMod wifiMod(application);

#define authProvider NULL

#ifdef WITH_RTC
#define PIN_ENA 23
#define PIN_CLK 21
#define PIN_DAT 22

#include "iotsaRtc.h"
IotsaRtcMod rtcMod(application, PIN_ENA, PIN_CLK, PIN_DAT, authProvider);
#endif

#ifdef WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application, authProvider);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, authProvider);
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

