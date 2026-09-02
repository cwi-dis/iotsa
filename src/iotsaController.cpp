#include "iotsa.h"
#include "iotsaController.h"

//
// Global variable definition
//
IotsaController iotsaController;

void IotsaController::tick() {
  if (_rebootAtMillis && millis() > _rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
}

void IotsaController::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  _rebootAtMillis = millis() + ms;
}
