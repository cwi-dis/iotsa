//
// BLE version of examples/Button, paired with examples/BLERinger: pressing
// the onboard BOOT button rings a remote device's Immediate Alert Service
// (IAS) over BLE. Standalone validation vehicle for
// IotsaImmediateAlertBLEClient -- no lissabon dependency.
//
// Board: esp32c3supermini -- reuses the onboard BOOT button (GPIO9) as the
// doorbell button, zero extra hardware. (GPIO9 is also the boot-mode
// strapping pin, same caveat as esp32dev's GPIO0 PRG button in
// lissabonSimpleRemote -- only matters if held during reset/power-up, not
// during normal runtime use.)
//
// The target device's name isn't known at compile time -- set it via REST
// (PUT /api/doorbell {"target": "<name>"}) or the web form, same as any
// other iotsa config field. Nothing rings until a target is set.
//
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaConfigFile.h"
#include "iotsaInput.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

#ifndef BUTTON_PIN
#define BUTTON_PIN 9 // BOOT button on esp32c3supermini/esp32c3devkit
#endif

IotsaApplication application("BLE Doorbell Button");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

#include "iotsaImmediateAlertBLEClient.h"

Button *doorbellButton = new Button(BUTTON_PIN, true, false, false);
Input *inputs[] = { doorbellButton };
IotsaInputMod inputMod(application, inputs, 1);

class BLEButtonMod : public IotsaModule {
public:
  BLEButtonMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaModule(_app, _auth),
    ringer(std::string())
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  void configLoad();
  void configSave();
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  void setTarget(const String& newTarget);
  IotsaImmediateAlertBLEClient ringer;
  String targetName;
  bool lastPressedState = false;
  bool wantsToRing = false;
  uint32_t giveUpAtMillis = 0;
  static const uint32_t connectTimeoutMillis = 10000;
};

void BLEButtonMod::setTarget(const String& newTarget) {
  if (newTarget == targetName) return;
  targetName = newTarget;
  if (targetName != "") {
    // IotsaImmediateAlertBLEClient has none of Lissabon::DimmerBLEClient's
    // setName()-with-register/unregister convenience (that's lissabon-
    // specific) -- register directly with the mod ourselves.
    ringer.setKnownName(std::string(targetName.c_str()));
    bleClientMod.addDevice(std::string(targetName.c_str()), &ringer);
  }
}

void BLEButtonMod::setup() {
  configLoad();
  NimBLEUUID iasServiceUUID(IotsaImmediateAlertBLE::serviceUUID);
  bleClientMod.setServiceFilter(iasServiceUUID);
}

void BLEButtonMod::lateSetup() {
  name = "doorbell";
  api.setup("doorbell", true, true, false, false);
}

String BLEButtonMod::info() {
  return "<p>BLE Doorbell Button: rings a remote Immediate Alert Service device over BLE when the BOOT button is pressed. See <a href='/api/doorbell'>/api/doorbell</a> for the REST API.</p>";
}

bool BLEButtonMod::getHandler(const char *path, JsonObject& reply) {
  reply["target"] = targetName;
  return true;
}

bool BLEButtonMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  String newTarget;
  if (getFromRequest<String>(request.as<JsonObject>(), "target", newTarget)) {
    setTarget(newTarget);
    configSave();
    return true;
  }
  return false;
}

void BLEButtonMod::configLoad() {
  IotsaConfigFileLoad cf("/config/doorbell.cfg");
  String storedTarget;
  cf.get("target", storedTarget, "");
  setTarget(storedTarget);
}

void BLEButtonMod::configSave() {
  IotsaConfigFileSave cf("/config/doorbell.cfg");
  cf.put("target", targetName);
}

void BLEButtonMod::loop() {
  bool nowPressed = doorbellButton->pressed;
  if (nowPressed && !lastPressedState) {
    if (targetName == "") {
      IotsaSerial.println("BLEButton: button pressed, but no target configured");
    } else {
      IotsaSerial.println("BLEButton: button pressed, ringing");
      wantsToRing = true;
      giveUpAtMillis = millis() + connectTimeoutMillis;
    }
  }
  lastPressedState = nowPressed;

  if (!wantsToRing) return;

  if (millis() > giveUpAtMillis) {
    IotsaSerial.println("BLEButton: giving up, could not reach the ringer");
    wantsToRing = false;
    return;
  }
  if (!ringer.available()) {
    return; // still waiting for a scan to find it
  }
  if (!ringer.isConnected()) {
    if (ringer.isDisconnecting()) return; // previous disconnect still settling
    if (!ringer.canConnect()) return; // radio busy (scanning or another connect), try again next loop()
    if (!ringer.connect()) return; // failed this attempt, retry until giveUpAtMillis
  }
  if (ringer.ring()) {
    IotsaSerial.println("BLEButton: rang the doorbell");
  } else {
    IotsaSerial.println("BLEButton: ring failed");
  }
  wantsToRing = false;
  ringer.disconnect();
}

// Instantiate the module, and install it in the framework
BLEButtonMod buttonMod(application);

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
