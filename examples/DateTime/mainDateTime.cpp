//
// A "DateTime" server: knows the time, with standard iotsa modules only, so
// there is no module of our own here.
//
// The NTP module gets the time from the network and keeps the timezone. Set
// both on the web page /ntpconfig or with PUT /api/ntpconfig. Your own module can then
// ask it for the time: ntpMod.isoTime(), ntpMod.localHours(), and so on (see
// iotsaNtp.h).
//
// The optional RTC module keeps the time in a DS1302 real-time clock, which
// has a battery, so after a reboot the device knows the time even before the
// network is there.
//

#include "iotsa.h"
#include "iotsaNtp.h"

// The DS1302's pins. These only exist on the classic ESP32: on the ESP8266,
// C3 and S3 they don't, and on the C3 GPIO21 is the console's TX pin. Define
// them for your own board to use the RTC there.
#ifdef CONFIG_IDF_TARGET_ESP32
#define RTC_PIN_ENA 23
#define RTC_PIN_CLK 21
#define RTC_PIN_DAT 22
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa DateTime Server");

// The real-time clock, if there are pins for it.
#ifdef RTC_PIN_ENA
#include "iotsaRtc.h"
IotsaRtcMod rtcMod(application, RTC_PIN_ENA, RTC_PIN_CLK, RTC_PIN_DAT);
#endif

// The time from the network.
IotsaNtpMod ntpMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
