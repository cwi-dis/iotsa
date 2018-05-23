#ifndef _IOTSALED_H_
#define _IOTSALED_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Adafruit_NeoPixel.h>

#ifdef IOTSA_WITH_API
#define IotsaLedModBaseMod IotsaApiMod
#else
#define IotsaLedModBaseMod IotsaMod
#endif

class IotsaLedMod : public IotsaLedModBaseMod, public IotsaStatusInterface {
public:
  IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t=NEO_GRB + NEO_KHZ800, IotsaAuthMod *_auth=NULL);
  void setup();
  void serverSetup();
  void loop();
  String info();
  void set(uint32_t _rgb, int _onDuration, int _offDuration, int _count);
  void showStatus();
protected:
  Adafruit_NeoPixel strip;
  uint32_t rgb;
  uint32_t nextChangeTime;
  int remainingCount;
  int onDuration;
  int offDuration;
  bool isOn;
  bool showingStatus;
};

#endif
