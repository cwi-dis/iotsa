#ifndef _IOTSANTP_H_
#define _IOTSANTP_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <WiFiUdp.h>

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

//  For converting UTC to local time this module can use a fixed offset, but also the timezone
// library from https://github.com/JChristensen/Timezone
// Define IOTSA_WITH_TIMEZONE_LIBRARY to use the latter.

#ifdef IOTSA_WITH_TIMEZONE_LIBRARY
class Timezone;
#endif

#ifdef IOTSA_WITH_API
#define IotsaNtpModBaseMod IotsaApiMod
#else
#define IotsaNtpModBaseMod IotsaMod
#endif

class IotsaNtpMod : public IotsaNtpModBaseMod {
public:
  using IotsaNtpModBaseMod::IotsaNtpModBaseMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif

  unsigned long utcTime();
  unsigned long localTime();
  int localSeconds();
  int localMinutes();
  int localHours();
  int localHours12();
  bool localIsPM();
  String isoTime();

  String ntpServer;
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
#ifdef IOTSA_WITH_TIMEZONE_LIBRARY
  Timezone *tz;
#endif
#ifdef IOTSA_WITH_TIMEZONE
  String tzDescription;
  void parseTimezone(const String& newDesc);
#else
  int minutesWestFromUtc;
  void _setupTimezone();
#endif
  void configLoad() override;
  void configSave() override;
  void handler();
  WiFiUDP udp;
  unsigned long nextNtpRequest; // When to send an NTP request
  unsigned long lastMillis; // To detect millis() rollover
  byte ntpPacket[NTP_PACKET_SIZE];
  bool gotInitialSync = false;

};

#endif
