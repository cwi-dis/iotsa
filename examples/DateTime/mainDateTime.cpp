//
// Demonstrates keeping a real-time clock (DS1302) in sync using NTP.
//

#include <Arduino.h>
#include "iotsa.h"

#define WITH_RTC    // Enable Realtime Clock support
#define WITH_NTP    // Use network time protocol to synchronize the clock.

IotsaApplication application("Iotsa DateTime Server");

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

void setup(void){
  application.setup();
  application.lateSetup();
}
 
void loop(void){
  application.loop();
}

