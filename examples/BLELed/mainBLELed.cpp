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

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

#ifndef WITHOUT_BATTERY
#define WITH_BATTERY
#endif
#ifdef ESP32
// ESP32-C3 and -S3 boards in this family don't have the 1:1 voltage-divider wiring
// on these pins -- gate on the chip target directly (CONFIG_IDF_TARGET_xxx, the
// ESP-IDF-provided macro, correct under both PlatformIO and Arduino IDE/arduino-cli)
// rather than via a separate opt-out build flag.
#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#define IOTSA_PIN_VBAT 36 // Undefine to disable battery voltage measurements. Use 1:1 voltage divider.
#define IOTSA_PIN_VUSB 37 // Undefine to disable USB voltage measurements. Use 1:1 voltage divider.
#endif
#ifdef CONFIG_IDF_TARGET_ESP32C3
#define IOTSA_PIN_DISABLESLEEP 9 // Define as pin to disable sleep (active low to disable)
#else
#define IOTSA_PIN_DISABLESLEEP 0 // Define as pin to disable sleep (active low to disable)
#endif
#endif // ESP32

#ifndef IOTSA_PIN_NEOPIXEL
#define IOTSA_PIN_NEOPIXEL 15 // Pulled down during boot on esp8266, can be used for led afterwards.
#endif

IotsaApplication application("Iotsa BLE LED Server");
#ifdef IOTSA_WITH_WIFI
IotsaWifiMod wifiMod(application);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#ifdef WITH_BATTERY
IotsaBatteryMod batteryMod(application);
#endif

#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
#endif

IotsaLedControlMod ledMod(application, IOTSA_PIN_NEOPIXEL);

// Standard setup() method, hands off most work to the application framework
void setup(void){
#ifdef WITH_BATTERY
#ifdef IOTSA_PIN_VBAT
  batteryMod.setPinVBat(IOTSA_PIN_VBAT);
#endif
#ifdef IOTSA_PIN_VUSB
  batteryMod.setPinVUSB(IOTSA_PIN_VUSB);
#endif
#ifdef IOTSA_PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(IOTSA_PIN_DISABLESLEEP);
#endif
  // As an example, we allow switching to configuration mode by sending a BLE command
  batteryMod.allowBLEConfigModeSwitch();
#endif
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
