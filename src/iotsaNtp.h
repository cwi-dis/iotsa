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
  void setup();
  void serverSetup();
  void loop();
  String info();
  
  unsigned long utcTime();
  unsigned long localTime();
  int localSeconds();
  int localMinutes();
  int localHours();
  int localHours12();
  bool localIsPM();

  String ntpServer;
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
#ifdef IOTSA_WITH_TIMEZONE_LIBRARY
  Timezone *tz;
  String tzDescription;
  void parseTimezone(const String& newDesc);
#else
  int minutesWestFromUtc;
#endif
  void configLoad();
  void configSave();
  void handler();
  WiFiUDP udp;
  unsigned long nextNtpRequest; // When to send an NTP request
  unsigned long lastMillis; // To detect millis() rollover
   byte ntpPacket[NTP_PACKET_SIZE];
  unsigned long utcTimeAtMillisEpoch;

};

#endif
