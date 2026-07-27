//
// A "Led" server, which allows control over a single NeoPixel (color,
// duration, on/off pattern). The led can be controlled through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//
// This is the application that is usually shipped with new iotsa boards.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaBattery.h"
#include "iotsaBLEServer.h"
#include "iotsaLedControlMod.h"

#define WITH_BATTERY
#ifdef ESP32
#ifndef WITHOUT_VOLTAGE
#define PIN_VBAT 36 // Undefine to disable battery voltage measurements. Use 1:1 voltage divider.
#define PIN_VUSB 37 // Undefine to disable USB voltage measurements. Use 1:1 voltage divider.
#endif
#ifdef ESP32C3
#define PIN_DISABLESLEEP 9 // Define as pin to disable sleep (active low to disable)
#else
#define PIN_DISABLESLEEP 0 // Define as pin to disable sleep (active low to disable)
#endif
#endif // ESP32

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 15 // Pulled down during boot on esp8266, can be used for led afterwards.
#endif

IotsaApplication application("Iotsa BLE LED Server");
#ifdef IOTSA_WITH_WIFI
IotsaWifiMod wifiMod(application);
#endif

#ifdef WITH_BATTERY
IotsaBatteryMod batteryMod(application);
#endif

#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
#endif

IotsaLedControlMod ledMod(application, NEOPIXEL_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT);
#endif
#ifdef PIN_VUSB
  batteryMod.setPinVUSB(PIN_VUSB);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  // As an example, we allow switching to configuration mode by sending a BLE command
  batteryMod.allowBLEConfigModeSwitch();
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
