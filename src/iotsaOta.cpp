#include "iotsaOta.h"
#ifdef IOTSA_WITH_OTA
#include <ArduinoOTA.h>
#ifdef IOTSA_WITH_BLE
#include "iotsaBLE.h"
#endif

void otaOnStart(IotsaApplication& app) {
  IFDEBUG IotsaSerial.println("ota: download started");
#ifdef IOTSA_WITH_BLE
  // Ask any BLE client work to hold off starting anything new for the
  // duration of the transfer (cwi-dis/iotsa#263) -- cleared in otaOnEnd()/
  // otaOnError(), whichever fires.
  IotsaBLERadioArbiter::holdOffNewWork(true);
#endif
  // The transfer holds loop() until it is done, so nothing renders the status
  // during it. Show solid cyan now; it stays until the reboot (cwi-dis/iotsa#259).
  iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_CYAN, 0, 0, 2000, "OTA update in progress");
  app.aboutToBlock();
  iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_OTA);
  iotsaController.feedWatchdog();
}

void otaOnProgress(unsigned int progress, unsigned int total) {
  IFDEBUG IotsaSerial.print("ota: got data ");
  IFDEBUG IotsaSerial.print(progress*100/total);
  IFDEBUG IotsaSerial.println("%");
  iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_OTA);
  iotsaController.extendCurrentMode();
  iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_CYAN, 0, 0, 2000, "OTA update in progress");  // re-armed per chunk (cwi-dis/iotsa#176)
  iotsaController.feedWatchdog();
}

void otaOnEnd() {
  IFDEBUG IotsaSerial.println("ota: download finished");
  iotsaBreadcrumbs.addBreadcrumb(IOTSA_CRUMB_REBOOT, 1);   // ArduinoOTA restarts the device itself
#ifdef IOTSA_WITH_BLE
  IotsaBLERadioArbiter::holdOffNewWork(false);
#endif
  iotsaController.feedWatchdog();
}

void otaOnError(int error) {
  IFDEBUG { IotsaSerial.print("ota: error: "); IotsaSerial.println(error); }
#ifdef IOTSA_WITH_BLE
  IotsaBLERadioArbiter::holdOffNewWork(false);
#endif
  iotsaController.feedWatchdog();
}

void IotsaOtaMod::setup() {
  // "OTA is available" is now derived by introspection -- IotsaRunmodeMod checks
  // for a module named "ota" (cwi-dis/iotsa#106); no iotsaConfig.otaEnabled flag.
  _startIfReady();
}

// Starts listening for OTA uploads once the device is in OTA mode and the
// TCP/IP stack is up (ArduinoOTA.begin() binds a UDP socket, see
// IotsaHttpServiceMod::_startIfReady(), cwi-dis/iotsa#239). Called every loop().
void IotsaOtaMod::_startIfReady() {
  if (_started || !iotsaStatus.networkStackUp) return;
  if (iotsaController.currentMode() != IOTSA_MODE_OTA) return;
  IotsaSerial.println("OTA-update enabled");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(iotsaConfig.hostName.c_str());
  ArduinoOTA.onStart([this]() { otaOnStart(app); });
  ArduinoOTA.onProgress(otaOnProgress);
  ArduinoOTA.onEnd(otaOnEnd);
  ArduinoOTA.onError(otaOnError);
  ArduinoOTA.begin();
  _started = true;
}

void IotsaOtaMod::lateSetup() {
  name = "ota";
}

void IotsaOtaMod::loop() {
  _startIfReady();
  if (!_started || iotsaController.currentMode() != IOTSA_MODE_OTA) return;
  ArduinoOTA.handle();
}

#ifdef IOTSA_WITH_WEB
String IotsaOtaMod::info() {
  String rv;
  if (iotsaController.currentMode() == IOTSA_MODE_OTA) {
    rv = "<p>Over the air (OTA) programming is enabled, will timeout in " + String((iotsaController.currentModeEndTime() - millis())/1000) + " seconds.</p>";
  } else if (iotsaController.requestedMode() == IOTSA_MODE_OTA) {
  	rv = "<p>Over the air (OTA) programming has been requested. Enable within " + String((iotsaController.requestedModeEndTime() - millis())/1000) + " seconds by power cycling";
    if (iotsaController.rcmInteractionDescription()) {
      rv += " or ";
      rv += iotsaController.rcmInteractionDescription();
    }
    rv += ".</p>";
  } else {
    rv = "<p>Over the air (OTA) programming possible, visit <a href=\"/config\">/config</a> to enable.</p>";
  }
  return rv;
}
#endif
#endif // IOTSA_WITH_OTA
