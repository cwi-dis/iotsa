#include "iotsa.h"
#include "iotsaController.h"

//
// Global variable definition
//
IotsaController iotsaController;

// The mode machine and radio/sleep policy moved into their own objects
// (cwi-dis/iotsa#106 step 5a). IotsaController is now just: seed the sub-policies
// at begin(), tick() them, and the deferred-reboot timer.

void IotsaController::begin() {
  _radio.seedFromBootPolicy(
    iotsaConfig.wifiDisabledOnBoot,
#ifdef IOTSA_WITH_BLE
    iotsaConfig.bleDisabledOnBoot
#else
    true   // no BLE -> "disabled on boot" is vacuously true
#endif
  );
  _modes.begin(iotsaStatus.wasHardwareReset());
}

void IotsaController::tick() {
  if (_rebootAtMillis && millis() > _rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
  _modes.tick();
}

void IotsaController::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  _rebootAtMillis = millis() + ms;
}
