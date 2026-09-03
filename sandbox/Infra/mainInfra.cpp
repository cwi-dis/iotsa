//
// Infra -- minimal scaffold for infrastructure / incoming-protocol work
// (cwi-dis/iotsa#106 and the rest of #199 layer 1a).
//
// The deliberate inverse of tests/KitchenSink: every infrastructure module and
// every incoming protocol, but exactly ONE trivial "application" module
// (IotsaNothingMod). KitchenSink's pile of application modules is pure noise when
// the thing under test is WiFi / config / runmode / the transports -- it just
// adds serial spam, flash bloat and build time.
//
// IotsaConfigMod is not declared here: it is ensured unconditionally by
// IotsaApplication::setup() (cwi-dis/iotsa#195), which this sketch also exercises.
//
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaBattery.h"
#include "iotsaOta.h"
#include "iotsaLogger.h"
#include "iotsaFilesBackup.h"
#include "iotsaNothing.h"
#ifdef IOTSA_PIN_NEOPIXEL
#include "iotsaLed.h"
#endif

IotsaApplication application("Iotsa Infra test rig");

IotsaWifiMod wifiMod(application);
IotsaBatteryMod batteryMod(application);
IotsaOtaMod otaMod(application);
IotsaLoggerMod loggerMod(application);
IotsaFilesBackupMod filesBackupMod(application);
#ifdef IOTSA_PIN_NEOPIXEL
// Only when the board definition says this board has a NeoPixel -- then the
// status LED (and getStatusColor()) is exercised too.
IotsaLedMod ledMod(application, IOTSA_PIN_NEOPIXEL);
#endif

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
IotsaBLEServerMod bleserverMod(application);
#endif

// The one and only application module.
IotsaNothingMod nothingMod(application);

void setup(void) {
  application.setup();
  application.lateSetup();
}

void loop(void) {
  application.loop();
}
