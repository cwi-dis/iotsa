#ifndef _IOTSARTC_H_
#define _IOTSARTC_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Ds1302.h>

#ifdef IOTSA_WITH_API
#define IotsaRtcModBaseMod IotsaApiMod
#else
#define IotsaRtcModBaseMod IotsaMod
#endif

class IotsaRtcMod : public IotsaRtcModBaseMod {
public:
  IotsaRtcMod(IotsaApplication &_app, uint8_t pin_ena, uint8_t pin_clk, uint8_t pin_dat, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaRtcModBaseMod(_app, _auth, early),
    ds1302(pin_ena, pin_clk, pin_dat)
  {

  }
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif

  const char *isoTime();
  bool setIsoTime(const char *time);
  bool setIsoTime(String time) { return setIsoTime(time.c_str()); }
  int localSeconds();
  int localMinutes();
  int localHours();
  int localHours12();
  bool localIsPM();

protected:
  Ds1302 ds1302;
  Ds1302::DateTime currentTime;
  uint32_t currentTimeMillis = 0;
  void _updateCurrentTime();
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
  void configLoad() override;
  void configSave() override;
  void handler();
  void _updateSysTime();
  void _updateFromSysTime();
  uint32_t nextUpdateMillis;
};

#endif
