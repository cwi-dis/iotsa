//
// Demonstrates keeping a real-time clock (DS1302) in sync using NTP.
//

#include <Arduino.h>
#include "iotsa.h"
#include "iotsaWifi.h"

#define WITH_RTC    // Enable Realtime Clock support
#define WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

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

void setup(void){
  application.setup();
  application.serverSetup();
}
 
void loop(void){
  application.loop();
}

