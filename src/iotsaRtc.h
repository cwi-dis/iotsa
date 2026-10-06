#ifndef _IOTSARTC_H_
#define _IOTSARTC_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Ds1302.h>

class IotsaRtcMod : public IotsaModule {
public:
  IotsaRtcMod(IotsaApplication &_app, uint8_t pin_ena, uint8_t pin_clk, uint8_t pin_dat, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaModule(_app, _auth, early),
    ds1302(pin_ena, pin_clk, pin_dat)
  {

  }
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif

  // The DS1302 keeps UTC. For local time, ask IotsaNtpMod (which owns the
  // timezone): this module is a battery-backed backup for the system clock,
  // seeding it at boot and saving it periodically (cwi-dis/iotsa#104).
  const char *isoTime();  // "YYYY-MM-DDTHH:MM:SSZ"
  bool setIsoTime(const char *time);  // UTC; a trailing "Z" is optional
  bool setIsoTime(String time) { return setIsoTime(time.c_str()); }

protected:
  Ds1302 ds1302;
  Ds1302::DateTime currentTime;
  uint32_t currentTimeMillis = 0;
  void _updateCurrentTime();
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
  void _updateSysTime();
  void _updateFromSysTime();
  uint32_t nextUpdateMillis;
};

#endif
