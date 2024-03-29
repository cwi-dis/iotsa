#include "iotsaOta.h"
#include <ArduinoOTA.h>

#ifdef ESP32
#define optFeedWatchdog()
#else
#define optFeedWatchdog() ESP.wdtFeed()
#endif

void otaOnStart() {
  IFDEBUG IotsaSerial.println("ota: download started");
  optFeedWatchdog();
}

void otaOnProgress(unsigned int progress, unsigned int total) {
//  if (app.status) app.status->showStatus();
  IFDEBUG IotsaSerial.print("ota: got data ");
  IFDEBUG IotsaSerial.print(progress*100/total);
  IFDEBUG IotsaSerial.println("%");
  iotsaConfig.extendCurrentMode();
  optFeedWatchdog();
}

void otaOnEnd() {
  IFDEBUG IotsaSerial.println("ota: download finished");
  optFeedWatchdog();
}

void otaOnError(int error) {
  IFDEBUG { IotsaSerial.print("ota: error: "); IotsaSerial.println(error); }
  optFeedWatchdog();
}

void IotsaOtaMod::setup() {
  iotsaConfig.otaEnabled = true;
  if (iotsaConfig.configurationMode == IOTSA_MODE_OTA) {
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

void IotsaOtaMod::serverSetup() {
}

void IotsaOtaMod::loop() {
  if (iotsaConfig.configurationMode == IOTSA_MODE_OTA) {
    if (!started) _start();
    ArduinoOTA.handle();
  }
}

#ifdef IOTSA_WITH_WEB
String IotsaOtaMod::info() {
  String rv;
  if (iotsaConfig.configurationMode == IOTSA_MODE_OTA) {
    rv = "<p>Over the air (OTA) programming is enabled, will timeout in " + String((iotsaConfig.configurationModeEndTime - millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_OTA) {
  	rv = "<p>Over the air (OTA) programming has been requested. Enable within " + String((iotsaConfig.nextConfigurationModeEndTime - millis())/1000) + " seconds by power cycling";
    if (iotsaConfig.rcmInteractionDescription) {
      rv += " or ";
      rv += iotsaConfig.rcmInteractionDescription;
    }
    rv += ".</p>";
  } else {
    rv = "<p>Over the air (OTA) programming possible, visit <a href=\"/config\">/config</a> to enable.</p>";
  }
  return rv;
}
#endif