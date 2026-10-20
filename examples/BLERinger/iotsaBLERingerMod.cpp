#include "iotsaBLERingerMod.h"

// Implementation of the BLE ringer module. It has no settings, so no web
// page, REST interface or configuration: only the BLE service.

void IotsaBLERingerMod::setup() {
  alertServer.setup();
}

// loop() is empty (in the .h): alertServer calls alert() when it is rung.

// The alert goes to the status LED, as a pulse: a colour for a while, after
// which the LED shows the device's status again by itself.
void IotsaBLERingerMod::alert(uint8_t level) {
  if (level == IotsaImmediateAlertBLE::MildAlert) {
    iotsaStatus.setStatusPulse(0x000020, 0, 0, 1000, "mild alert");  // dim blue, 1 s
  } else if (level == IotsaImmediateAlertBLE::HighAlert) {
    iotsaStatus.setStatusPulse(0xff0000, 200, 200, 3000, "high alert");  // red, blinking, 3 s
  }
  // NoAlert: nothing to do, the pulse ends by itself.
}

String IotsaBLERingerMod::info() {
  return "<p>Rings over BLE: a BLEButton, or any BLE tool, can ring it with the Immediate Alert Service.</p>";
}
