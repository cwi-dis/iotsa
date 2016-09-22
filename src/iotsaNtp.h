#ifndef _WAPPNTP_H_
#define _WAPPNTP_H_
#include "Wapp.h"
#include <WiFiUdp.h>

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

class WappNtpMod : public WappMod {
public:
  WappNtpMod(Wapplication &_app) : WappMod(_app) {}
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
  int minutesWestFromUtc;
  
protected:
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
