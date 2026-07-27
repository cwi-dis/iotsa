//
// Server that provides a web interface to a DHT21 temperature sensor.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaTemperatureMod.h"

#define DHT_PIN 13
#define DHT_TYPE DHT21

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Temperature Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

IotsaTemperatureMod temperatureMod(application, DHT_PIN, DHT_TYPE);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
