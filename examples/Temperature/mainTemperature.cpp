//
// Server that provides a web interface to a DHT21 temperature sensor.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"
#include "iotsaTemperatureMod.h"

#define DHT_PIN 13
#define DHT_TYPE DHT21

IotsaApplication application("Iotsa Temperature Server");
IotsaWifiMod wifiMod(application);
IotsaOtaMod otaMod(application);
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
