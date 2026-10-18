//
// Demonstrates keeping a real-time clock (DS1302) in sync using NTP.
//

#include <Arduino.h>
#include "iotsa.h"

// Realtime Clock support. The pins below only exist on the classic ESP32: on the
// ESP8266, C3 and S3 they don't, and on the C3 GPIO21 is the console's TX pin.
// Pick pins for your own board to use it there.
#ifdef CONFIG_IDF_TARGET_ESP32
#define WITH_RTC
#endif
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

