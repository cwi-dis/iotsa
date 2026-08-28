//
// A "Led" server: the reference example for both halves of what iotsa's LED support
// can do, in one sketch.
//
// - LED control: full flash-pattern control over a single NeoPixel (color, on/off
//   duration, repeat count) via a web UI, REST, and/or CoAP calls; plus, when built
//   with IOTSA_WITH_BLE, a solid-color-only BLE characteristic. The web interface can
//   be disabled by building iotsa with IOTSA_WITHOUT_WEB.
// - Battery/sleep: also the reference example for battery-powered devices -- it wires
//   up iotsaBattery (VBAT/VUSB voltage measurement, disable-sleep pin) and shows how to
//   let a BLE command switch the device into config mode, for low-power operation
//   without a permanent WiFi/web connection.
//
// Folded together from the formerly-separate Led/BLELed examples, which had drifted
// into two different feature sets by accident rather than design -- see cwi-dis/iotsa#222.
//
// Optional startup blink: on a build with no network reachability at all (see the
// "nonetworking" test variant), there's otherwise no way to tell from the outside
// whether the device booted successfully. Opt in with
// -DIOTSA_STARTUP_BLINK_COUNT=<n>; off by default.
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

IotsaApplication application("Iotsa LED Server");
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
#ifdef IOTSA_STARTUP_BLINK_COUNT
  // Arm a self-test blink pattern; ledMod.loop() (driven from the main loop() below)
  // advances it asynchronously from here.
  ledMod.set(0xffffff, 150, 150, IOTSA_STARTUP_BLINK_COUNT);
#endif
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
