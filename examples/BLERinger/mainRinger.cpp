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
#include "iotsa.h"
#include "iotsaWifi.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("BLE Ringer");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEServer.h"
IotsaBLEServerMod bleServerMod(application);

#include "iotsaImmediateAlertBLEServer.h"
#include "NeoPixelAlert.h"

NeoPixelAlert alertLight;

void onAlertLevelChanged(uint8_t level) {
  alertLight.setAlertLevel(level);
}

IotsaImmediateAlertBLEServer alertServer(onAlertLevelChanged);

class BLERingerMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override {
    alertLight.setup();
    alertServer.setup();
  }
  void loop() override {
    alertLight.loop();
  }
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
