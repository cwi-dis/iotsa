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
#include "iotsaBattery.h"
#include "iotsaLogger.h"
#include "iotsaFilesBackup.h"
#include "iotsaNothing.h"

IotsaApplication application("Iotsa Infra test rig");

IotsaBatteryMod batteryMod(application);
IotsaLoggerMod loggerMod(application);
IotsaFilesBackupMod filesBackupMod(application);
// The status LED (and iotsaStatus.statusColor()) is exercised too on boards
// whose definition has one: iotsa creates it automatically (cwi-dis/iotsa#272).

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
