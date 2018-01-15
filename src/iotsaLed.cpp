#include "iotsaLed.h"
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

// Helper function: get color to show current status of module.
static uint32_t _getStatusColor() {
  if (!WiFi.isConnected()) return 0x3f1f00; // Orange: not connected to WiFi
  if (tempConfigurationMode == TMPC_RESET) return 0x3f0000; // Red: Factory reset mode
  if (tempConfigurationMode == TMPC_CONFIG) return 0x3f003f;	// Magenta: user-requested configuration mode
  if (tempConfigurationMode == TMPC_OTA) return 0x003f3f;	// Cyan: OTA mode
  if (configurationMode) return 0x3f3f00; // Yellow: configuration mode (not user requested)
  return 0; // Off: all ok.
}

IotsaLedMod::IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t, IotsaAuthMod *_auth)
:	IotsaMod(_app, _auth, true),
	strip(1, pin, t),
	rgb(0),
	nextChangeTime(0)
{
	app.status = this;
}

void IotsaLedMod::setup() {
  strip.begin();
  strip.show();
}

void IotsaLedMod::loop() {
  //IotsaSerial.println("led in");
  if (nextChangeTime == 0 || millis() < nextChangeTime) {
	//IotsaSerial.println("led early out");
  	return;
  }
  // We need to change the LED.
  if (isOn) {
  	// Need to turn it off
  	strip.setPixelColor(0, 0);
  	strip.show();
  	isOn = false;
  	if (remainingCount <= 0) {
  		// We are done with the pattern.
  		nextChangeTime = 0;
		//IotsaSerial.println("led done");
  		return;
	}
	nextChangeTime = millis() + offDuration;
  } else {
  	if (showingStatus) {
  		rgb = _getStatusColor();
	}
  	// Turn it on, set next change time
  	strip.setPixelColor(0, rgb);
  	strip.show();
  	isOn = true;
  	nextChangeTime = millis() + onDuration;
    if (!showingStatus) remainingCount--;
  }
  //IotsaSerial.println("led return");
}

void IotsaLedMod::serverSetup() {
	// Do nothing
}

String IotsaLedMod::info() {
	return "";
}

void IotsaLedMod::set(uint32_t _rgb, int _onDuration, int _offDuration, int _count) {
  showingStatus = false;
  rgb = _rgb;
  onDuration = _onDuration;
  offDuration = _offDuration;
  remainingCount = _count;
  isOn = false;
  nextChangeTime = millis();
}

void IotsaLedMod::showStatus() {
  showingStatus = true;
  onDuration = 500;
  offDuration = 500;
  isOn = false;
  remainingCount = 0x7fff;
  nextChangeTime = millis();
  // Call loop here to update immedeately
  loop();
}
