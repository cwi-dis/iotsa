//
// A "temperature" server: reads temperature and humidity from a DHT21 sensor,
// on a web page and with GET /api/temperature.
//
// Connect the sensor's data line to DHT_PIN. For another sensor of the
// family (DHT11, DHT22) change DHT_TYPE.
//
// This is the first example that uses another library to talk to the
// hardware: Adafruit's "DHT sensor library", which needs "Adafruit Unified
// Sensor" too. PlatformIO installs both by itself (see lib_deps in
// platformio.ini). In the Arduino IDE, install them with the Library Manager.
//

#include "iotsa.h"
#include "iotsaTemperatureMod.h"

#define DHT_PIN 13
#define DHT_TYPE DHT21

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Temperature Server");

// Our module, see iotsaTemperatureMod.cpp. It gets the pin and sensor type
// from here, so the module itself doesn't need to know about the hardware.
IotsaTemperatureMod temperatureMod(application, DHT_PIN, DHT_TYPE);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
