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
// tests/Transports builds this sketch with the transport combinations
// (onlycoap, onlyhttps, onlyble, nonetworking, ...).
//
// Optional startup blink: on a build with no network reachability at all (the
// "nonetworking" variant), there's otherwise no way to tell from the outside
// whether the device booted successfully. Opt in with
// -DIOTSA_STARTUP_BLINK_COUNT=<n>; off by default. Visible only on boards with
// a status LED.
//
#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaLogger.h"
#include "iotsaFilesBackup.h"
#include "iotsaNothing.h"

IotsaApplication application("Iotsa Infra test rig");

IotsaBatteryMod batteryMod(application);
#ifdef IOTSA_HAS_WEBSERVER
// Web-server-only modules: left out by the builds without HTTP/HTTPS.
IotsaLoggerMod loggerMod(application);
IotsaFilesBackupMod filesBackupMod(application);
#endif
// The status LED (and iotsaStatus.statusColor()) is exercised too on boards
// whose definition has one: iotsa creates it automatically (cwi-dis/iotsa#272).

// The one and only application module.
IotsaNothingMod nothingMod(application);

void setup(void) {
  application.setup();
  application.lateSetup();
#ifdef IOTSA_STARTUP_BLINK_COUNT
  // Arm a self-test blink pattern via the status-pulse channel (cwi-dis/iotsa#176);
  // the status LED's loop() renders it. 150+150 per blink.
  iotsaStatus.setStatusPulse(0xffffff, 150, 150, IOTSA_STARTUP_BLINK_COUNT * 300, "startup blink");
#endif
}

void loop(void) {
  application.loop();
}
