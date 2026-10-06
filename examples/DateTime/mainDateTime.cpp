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


#ifdef WITH_RTC
#define PIN_ENA 23
#define PIN_CLK 21
#define PIN_DAT 22

#include "iotsaRtc.h"
IotsaRtcMod rtcMod(application, PIN_ENA, PIN_CLK, PIN_DAT);
#endif

#ifdef WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

void setup(void){
  application.setup();
  application.lateSetup();
}
 
void loop(void){
  application.loop();
}

