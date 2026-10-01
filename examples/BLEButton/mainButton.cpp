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
// (PUT /api/doorbell {"name": "<name>"}) or the web form, same as any other
// iotsa config field. Nothing rings until a target is set. Config/REST/web
// for the target's identity itself (name, resolved address, found/connected
// status, renaming) all come from IotsaImmediateAlertBLEClient's own
// IotsaApiModObject surface (cwi-dis/iotsa#268) -- BLEButtonMod only adds
// the button-press/ring-attempt state machine on top.
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
    // Pass bleClientMod as owner right away: ringer.retarget() (called from
    // configLoad()/putHandler()/webHandler() below, all the way down in
    // IotsaBLEClientDevice) can then self-register on the very first real
    // name, no separate addDevice() call needed here (cwi-dis/iotsa#268).
    ringer(std::string(), "", &bleClientMod)
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
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
private:
  void startRingAttempt();
  IotsaImmediateAlertBLEClient ringer;
  // Outcome of the most recent ring attempt, shown on the web page -- loop()
  // drives an asynchronous connect/ring state machine, so a ringnow request
  // can't just return the result synchronously in webHandler(); this is
  // read back on the next page load instead.
  String lastRingStatus;
  bool lastPressedState = false;
  bool wantsToRing = false;
  uint32_t giveUpAtMillis = 0;
  static const uint32_t connectTimeoutMillis = 10000;
};

void BLEButtonMod::setup() {
  configLoad();
}

void BLEButtonMod::lateSetup() {
  name = "doorbell";
  api.setup("doorbell", true, true);
}

String BLEButtonMod::info() {
  return "<p>BLE Doorbell Button: rings a remote Immediate Alert Service device over BLE when the BOOT button is pressed. See <a href='/doorbell'>/doorbell</a> to configure, or <a href='/api/doorbell'>/api/doorbell</a> for the REST API.</p>";
}

#ifdef IOTSA_WITH_WEB
void BLEButtonMod::webHandler() {
  IotsaWebServer *server = api.webService->server;
  if (ringer.formHandler_args(server, "target", true)) {
    configSave();
  }
  if (server->hasArg("ringnow")) {
    startRingAttempt();
  }
  String message = "<html><head><title>BLE Doorbell Button</title></head><body><h1>BLE Doorbell Button</h1>";
  message += "<form method='get'>";
  ringer.formHandler_fields(message, "Ring target: ", "target", true);
  message += "<input type='submit' value='Set'>";
  message += "</form>";
  message += "<form method='get'><input type='hidden' name='ringnow' value='1'>";
  message += "<input type='submit' value='Ring now'";
  if (!ringer.available()) message += " disabled"; // nothing to ring yet
  message += "></form>";
  if (lastRingStatus != "") {
    message += "<p>Last ring attempt: " + lastRingStatus + "</p>";
  }
  message += "<p>Or just fetch <code>/doorbell?ringnow=1</code> directly -- no form needed.</p>";
  server->send(200, "text/html", message);
}
#endif // IOTSA_WITH_WEB

bool BLEButtonMod::getHandler(const char *path, JsonObject& reply) {
  ringer.getHandler(reply);
  return true;
}

bool BLEButtonMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (ringer.putHandler(request)) {
    configSave();
    return true;
  }
  return false;
}

void BLEButtonMod::configLoad() {
  IotsaConfigFileLoad cf("/config/doorbell.cfg");
  ringer.configLoad(cf, "target");
}

void BLEButtonMod::configSave() {
  IotsaConfigFileSave cf("/config/doorbell.cfg");
  ringer.configSave(cf, "target");
}

void BLEButtonMod::startRingAttempt() {
  if (ringer.getName().empty()) {
    IotsaSerial.println("BLEButton: ring requested, but no target configured");
    lastRingStatus = "no target configured";
    return;
  }
  wantsToRing = true;
  lastRingStatus = "ringing...";
  giveUpAtMillis = millis() + connectTimeoutMillis;
}

void BLEButtonMod::loop() {
  bool nowPressed = doorbellButton->pressed;
  if (nowPressed && !lastPressedState) {
    IotsaSerial.println("BLEButton: button pressed");
    startRingAttempt();
  }
  lastPressedState = nowPressed;

  if (!wantsToRing) return;

  if (millis() > giveUpAtMillis) {
    IotsaSerial.println("BLEButton: giving up, could not reach the ringer");
    lastRingStatus = "gave up: target not reachable";
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
    lastRingStatus = "rang successfully";
  } else {
    IotsaSerial.println("BLEButton: ring failed");
    lastRingStatus = "ring command failed";
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
