#ifndef _IOTSANTP_H_
#define _IOTSANTP_H_
#include "iotsa.h"
#include "iotsaApi.h"

// Time synchronization and timezone. The actual NTP client is the platform's
// own (lwIP SNTP, started via configTime()/configTzTime()); this module only
// holds the server name and the POSIX TZ description, and offers local-time
// helpers on top of libc's localtime().
class IotsaNtpMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif

  unsigned long utcTime();  // Seconds since 1-Jan-1970, UTC (unix time)
  int localSeconds();
  int localMinutes();
  int localHours();
  int localHours12();
  bool localIsPM();
  String isoTime();         // Local time, "YYYY-MM-DDTHH:MM:SS"

  String ntpServer;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  String tzDescription;
  void parseTimezone(const String& newDesc);
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
};

#endif
