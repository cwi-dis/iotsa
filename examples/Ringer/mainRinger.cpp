// A web interface to a buzzer. Has a web interface and a REST interface (and/or COAP interface, based on
// iotsa configuration options).
// The (user-centric) web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB
//
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"
#include "iotsaAlarmMod.h"

IotsaApplication application("Ringer Server");
IotsaWifiMod wifiMod(application);  // wifi is always needed
IotsaOtaMod otaMod(application);    // we want OTA for updating the software (will not work with esp-201)
IotsaAlarmMod alarmMod(application);

//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.serverSetup();
}

void loop(void) {
  application.loop();
}
