//
// A "Ringer" server: sounds a buzzer when asked. On the web page /alarm, with
// PUT /api/alarm (REST, or CoAP when built with IOTSA_WITH_COAP), and with the
// trigger /ring?alarm=20. Together with the Button example this is a
// doorbell: the button calls the trigger. See iotsaAlarmMod.cpp.
//

#include "iotsa.h"
#include "iotsaAlarmMod.h"

#ifndef ALARM_PIN
#define ALARM_PIN 4  // GPIO4 drives the buzzer
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Ringer Server");

// Our module, see iotsaAlarmMod.cpp. It gets the pin from here.
IotsaAlarmMod alarmMod(application, ALARM_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
