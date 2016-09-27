#include "iotsaOta.h"

#include <ArduinoOTA.h>

void otaOnStart() {
  IFDEBUG Serial.println("ota: download started");
  tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  ESP.wdtFeed();
}

void otaOnProgress(unsigned int progress, unsigned int total) {
  IFDEBUG Serial.print("ota: got data ");
  IFDEBUG Serial.print(progress*100/total);
  IFDEBUG Serial.println("%");
  tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  ESP.wdtFeed();
}

void otaOnEnd() {
  IFDEBUG Serial.println("ota: download finished");
  tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  ESP.wdtFeed();
}

void otaOnError(int error) {
  IFDEBUG { Serial.print("ota: error: "); Serial.println(error); }
  ESP.wdtFeed();
}

void IotsaOtaMod::setup() {
  app.enableOta();
  if (tempConfigurationMode == TMPC_OTA) {
	Serial.println("OTA-update enabled");
	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(hostName.c_str());
	ArduinoOTA.onStart(otaOnStart);
	ArduinoOTA.onProgress(otaOnProgress);
	ArduinoOTA.onEnd(otaOnEnd);
	ArduinoOTA.onError(otaOnError);
	ArduinoOTA.begin();
	tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  }
}

void IotsaOtaMod::serverSetup() {
}

void IotsaOtaMod::loop() {
  if (tempConfigurationMode == TMPC_OTA) ArduinoOTA.handle();
}

String IotsaOtaMod::info() {
  String rv;
  if (tempConfigurationMode == TMPC_OTA) {
    rv = "<p>Over the air (OTA) programming is enabled, will timeout in " + String((tempConfigurationModeTimeout - millis())/1000) + " seconds.</p>";
  } else {
    rv = "<p>Over the air (OTA) programming possible, visit <a href=\"/wificonfig\">/wificonfig</a> to enable.</p>";
  }
  return rv;
}
