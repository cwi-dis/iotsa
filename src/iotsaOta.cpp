#include "iotsaOta.h"
#include <ArduinoOTA.h>
#ifdef IOTSA_WITH_BLE
#include "iotsaBLE.h"
#endif

#ifdef ESP32
#define optFeedWatchdog()
#else
#define optFeedWatchdog() ESP.wdtFeed()
#endif

void otaOnStart() {
  IFDEBUG IotsaSerial.println("ota: download started");
#ifdef IOTSA_WITH_BLE
  // Ask any BLE client work to hold off starting anything new for the
  // duration of the transfer (cwi-dis/iotsa#263) -- cleared in otaOnEnd()/
  // otaOnError(), whichever fires.
  IotsaBLERadioArbiter::holdOffNewWork(true);
#endif
  optFeedWatchdog();
}

void otaOnProgress(unsigned int progress, unsigned int total) {
  IFDEBUG IotsaSerial.print("ota: got data ");
  IFDEBUG IotsaSerial.print(progress*100/total);
  IFDEBUG IotsaSerial.println("%");
  iotsaController.extendCurrentMode();
  iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_CYAN, 0, 0, 2000, "OTA update in progress");  // re-armed per chunk (cwi-dis/iotsa#176)
  optFeedWatchdog();
}

void otaOnEnd() {
  IFDEBUG IotsaSerial.println("ota: download finished");
#ifdef IOTSA_WITH_BLE
  IotsaBLERadioArbiter::holdOffNewWork(false);
#endif
  optFeedWatchdog();
}

void otaOnError(int error) {
  IFDEBUG { IotsaSerial.print("ota: error: "); IotsaSerial.println(error); }
#ifdef IOTSA_WITH_BLE
  IotsaBLERadioArbiter::holdOffNewWork(false);
#endif
  optFeedWatchdog();
}

void IotsaOtaMod::setup() {
  // "OTA is available" is now derived by introspection -- IotsaRunmodeMod checks
  // for a module named "ota" (cwi-dis/iotsa#106); no iotsaConfig.otaEnabled flag.
  if (iotsaController.currentMode() == IOTSA_MODE_OTA) {
    _start();
  }
}

void IotsaOtaMod::_start() {
  IotsaSerial.println("OTA-update enabled");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(iotsaConfig.hostName.c_str());
  ArduinoOTA.onStart(otaOnStart);
  ArduinoOTA.onProgress(otaOnProgress);
  ArduinoOTA.onEnd(otaOnEnd);
  ArduinoOTA.onError(otaOnError);
  ArduinoOTA.begin();
  started = true;
}

void IotsaOtaMod::lateSetup() {
  name = "ota";
}

void IotsaOtaMod::loop() {
  if (iotsaController.currentMode() == IOTSA_MODE_OTA) {
    if (!started) _start();
    ArduinoOTA.handle();
  }
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