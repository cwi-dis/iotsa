//
// BLE version of examples/Ringer: instead of a REST-triggered buzzer, this
// exposes the Bluetooth SIG-standard Immediate Alert Service (IAS) as a BLE
// peripheral, showing the alert on the board's onboard NeoPixel. Standalone
// validation vehicle for IotsaImmediateAlertBLEServer/Client -- no lissabon
// dependency.
//
// Board: esp32c3devkit (Espressif ESP32-C3-DevKitM-1) -- uses its onboard
// addressable RGB LED (GPIO8) as the alert indicator, zero extra hardware.
//
// The alert is shown via IotsaLedMod/iotsaStatus.setStatusPulse(), the same
// generic status-display mechanism every other iotsa app uses (see
// examples/Button) -- not a bespoke NeoPixel driver. That means this ringer
// becomes just another status-pulse consumer if/when a real buzzer is added
// (combining with examples/Ringer) or other status consumers exist.
//
#include "iotsa.h"

IotsaApplication application("BLE Ringer");

#include "iotsaBLEServer.h"
IotsaBLEServerMod bleServerMod(application);

// The alert is shown on the board's status LED, which iotsa creates
// automatically (cwi-dis/iotsa#272) -- e.g. esp32c3devkit's onboard NeoPixel.

#include "iotsaImmediateAlertBLEServer.h"

void onAlertLevelChanged(uint8_t level) {
  if (level == IotsaImmediateAlertBLE::MildAlert) {
    iotsaStatus.setStatusPulse(0x000020, 0, 0, 1000, "IAS mild alert"); // dim blue, solid, 1s
  } else if (level == IotsaImmediateAlertBLE::HighAlert) {
    iotsaStatus.setStatusPulse(0xff0000, 200, 200, 3000, "IAS high alert"); // bright red, blinking, 3s
  }
  // NoAlert: nothing to actively cancel -- pulses expire on their own.
}

IotsaImmediateAlertBLEServer alertServer(onAlertLevelChanged);

class BLERingerMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override {
    alertServer.setup();
  }
  void loop() override {}
  String info() override {
    return "<p>BLE Ringer: exposes the Bluetooth SIG Immediate Alert Service (0x1802) over BLE.</p>";
  }
};

BLERingerMod ringerMod(application);

//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.lateSetup();
}

void loop(void) {
  application.loop();
}
