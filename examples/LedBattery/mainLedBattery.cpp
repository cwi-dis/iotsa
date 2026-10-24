//
// The Led example (read that one first) on a battery-powered device: it
// measures the battery and USB voltages, and it can sleep between uses to
// save power. A BLE client can wake it up into configuration mode, so it
// doesn't need WiFi all the time.
//
// Everything that is new compared to Led is marked "Battery:". The LED
// module itself is the same as in Led.
//
// Battery: this needs BLE, so an ESP32 built with IOTSA_WITH_BLE. With BLE,
// iotsa also includes the sleep support. PlatformIO sets the flag (see
// iotsa-build.json); the Arduino IDE has no easy way to set it.
//

#include "iotsa.h"
#include "iotsaLedControlMod.h"
#include "iotsaBattery.h"
#include "iotsaRunmode.h"

#ifndef IOTSA_WITH_BLE
#error "LedBattery needs IOTSA_WITH_BLE, and so an ESP32"
#endif

// Battery: which pins are used for what. These depend on how your board is
// wired, so check them.
#if defined(CONFIG_IDF_TARGET_ESP32)
// The classic ESP32 boards we use measure the battery and USB voltages on
// these pins, each through a 1:1 voltage divider. Leave out a define if your
// board doesn't have it.
#define BATTERY_PIN_VBAT 36
#define BATTERY_PIN_VUSB 37
#define BATTERY_PIN_DISABLESLEEP 0  // the BOOT button
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BATTERY_PIN_DISABLESLEEP 9  // the BOOT button
#else
#define BATTERY_PIN_DISABLESLEEP 0  // the BOOT button
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa LED Server");

// Battery: the battery module measures the voltages and shows them on its page
// /battery, over REST, and as the standard BLE battery service.
IotsaBatteryMod batteryMod(application);

// Battery: the runmode module decides when the device sleeps (configured on its
// page /runmode, off until you set a sleep mode there). The application
// creates one by itself, but we declare it here because we need to tell it
// two things in setup().
IotsaRunmodeMod runmodeMod(application);

// Our module, the same as in Led, see iotsaLedControlMod.cpp. It also is the
// status LED, so it needs the LED: the board's own status LED if it has one
// (the board defines IOTSA_PIN_LED), otherwise a NeoPixel whose data line you
// connect to GPIO15.
#ifdef IOTSA_PIN_LED
IotsaLedControlMod ledMod(application);
#else
IotsaLedControlMod ledMod(application, 15);
#endif

// Standard setup() method, hands off most work to the application framework
void setup(void){
  // Battery: tell the battery module where to measure.
#ifdef BATTERY_PIN_VBAT
  batteryMod.setPinVBat(BATTERY_PIN_VBAT);
#endif
#ifdef BATTERY_PIN_VUSB
  batteryMod.setPinVUSB(BATTERY_PIN_VUSB);
#endif
  // Battery: while this pin is LOW (the BOOT button held down) the device
  // doesn't go to sleep. Handy when you want to reach it.
  runmodeMod.setPinDisableSleep(BATTERY_PIN_DISABLESLEEP);
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
