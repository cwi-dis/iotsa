#include "iotsaLed.h"

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
  //Serial.println("led in");
  if (nextChangeTime == 0 || millis() < nextChangeTime) {
	//Serial.println("led early out");
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
		//Serial.println("led done");
  		return;
	}
	nextChangeTime = millis() + offDuration;
  } else {
  	// Turn it on, set next change time
  	strip.setPixelColor(0, rgb);
  	strip.show();
  	isOn = true;
  	nextChangeTime = millis() + onDuration;
   remainingCount--;
  }
  //Serial.println("led return");
}

void IotsaLedMod::serverSetup() {
	// Do nothing
}

String IotsaLedMod::info() {
	return "";
}

void IotsaLedMod::set(uint32_t _rgb, int _onDuration, int _offDuration, int _count) {
  rgb = _rgb;
  onDuration = _onDuration;
  offDuration = _offDuration;
  remainingCount = _count;
  isOn = false;
  nextChangeTime = millis();
}
