#ifndef _IOTSANTP_H_
#define _IOTSANTP_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <WiFiUdp.h>

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

class IotsaNtpMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
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
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
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
