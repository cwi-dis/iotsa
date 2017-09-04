#include "iotsaOta.h"

#include <ArduinoOTA.h>

void otaOnStart() {
  IFDEBUG IotsaSerial.println("ota: download started");
  ESP.wdtFeed();
}

void otaOnProgress(unsigned int progress, unsigned int total) {
  IFDEBUG IotsaSerial.print("ota: got data ");
  IFDEBUG IotsaSerial.print(progress*100/total);
  IFDEBUG IotsaSerial.println("%");
  if (millis() > tempConfigurationModeTimeout - 1000*CONFIGURATION_MODE_TIMEOUT) {
	  tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  }
  ESP.wdtFeed();
}

void otaOnEnd() {
  IFDEBUG IotsaSerial.println("ota: download finished");
  ESP.wdtFeed();
}

void otaOnError(int error) {
  IFDEBUG { IotsaSerial.print("ota: error: "); IotsaSerial.println(error); }
  ESP.wdtFeed();
}

void IotsaOtaMod::setup() {
  app.enableOta();
  if (tempConfigurationMode == TMPC_OTA) {
	IotsaSerial.println("OTA-update enabled");
	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(hostName.c_str());
	ArduinoOTA.onStart(otaOnStart);
	ArduinoOTA.onProgress(otaOnProgress);
	ArduinoOTA.onEnd(otaOnEnd);
	ArduinoOTA.onError(otaOnError);
	ArduinoOTA.begin();
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
  } else if (nextConfigurationMode == TMPC_OTA) {
  	rv = "<p>Over the air (OTA) programming has been requested. Power cycle within " + String((nextConfigurationModeTimeout - millis())/1000) + " seconds to enable.</p>";
  } else {
    rv = "<p>Over the air (OTA) programming possible, visit <a href=\"/wificonfig\">/wificonfig</a> to enable.</p>";
  }
  return rv;
}
