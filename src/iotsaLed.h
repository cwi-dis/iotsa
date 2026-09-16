#ifndef _IOTSALED_H_
#define _IOTSALED_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Adafruit_NeoPixel.h>

class IotsaLedMod : public IotsaModule {
public:
  IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t=NEO_GRB + NEO_KHZ800, IotsaAuthMod *_auth=NULL);
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  void set(uint32_t _rgb, int _onDuration, int _offDuration, int _count);
  // Cancel any in-flight set() pattern and resume polling iotsaStatus.statusColor()
  // immediately. Not part of any interface (cwi-dis/iotsa#176 removed the old
  // push-based IotsaStatusInterface) -- just a plain convenience method, kept
  // for callers (e.g. iotsaDoorOpener) that want to force-resume status display.
  void showStatus();
protected:
  Adafruit_NeoPixel strip;
  uint32_t rgb;
  uint32_t nextChangeTime;  // 0 = no set() pattern in flight -- poll status instead
  int remainingCount;
  int onDuration;
  int offDuration;
  bool isOn;
  uint32_t lastShownColor;  // dedup: only strip.show() when the polled colour changes
};

#endif
