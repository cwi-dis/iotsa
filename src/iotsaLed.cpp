#include "iotsaLed.h"
#include <ESP8266WiFi.h>

// Helper function: get color to show current status of module.
uint32_t _getStatusColor() {
  if (configurationMode || tempConfigurationMode == TMPC_CONFIG) return 0x3f003f;	// Pink: configuration mode
  if (tempConfigurationMode == TMPC_OTA) return 0x003f3f;	// Magenta: OTA mode
  if (!WiFi.isConnected()) return 0x3f1f00; // Orange: not connected to WiFi
  return 0x3f3f3f; // White: all ok.
}

IotsaLedMod::IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t)
:	IotsaMod(_app),
	strip(1, pin, t),
	rgb(0),
	nextChangeTime(0)
{
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
  onDuration = 10;
  offDuration = 4990;
  isOn = false;
  remainingCount = 0x7fff;
  nextChangeTime = millis();
}
