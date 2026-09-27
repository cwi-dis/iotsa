#include "NeoPixelAlert.h"

void NeoPixelAlert::setup() {
  strip = new Strip(1, IOTSA_PIN_NEOPIXEL);
  strip->Begin();
  strip->Show();
}

void NeoPixelAlert::setAlertLevel(uint8_t level) {
  RgbColor color(0, 0, 0);
  if (level == IotsaImmediateAlertBLE::MildAlert) {
    color = RgbColor(0, 0, 32); // dim blue
  } else if (level == IotsaImmediateAlertBLE::HighAlert) {
    color = RgbColor(255, 0, 0); // bright red
  }
  strip->SetPixelColor(0, color);
  strip->Show();
  if (level == IotsaImmediateAlertBLE::NoAlert) {
    alertEndAtMillis = 0;
  } else {
    alertEndAtMillis = millis() + alertDurationMillis;
  }
}

void NeoPixelAlert::loop() {
  if (alertEndAtMillis != 0 && millis() > alertEndAtMillis) {
    alertEndAtMillis = 0;
    strip->SetPixelColor(0, RgbColor(0, 0, 0));
    strip->Show();
  }
}
