#ifndef _IOTSANTP_H_
#define _IOTSANTP_H_
#include "iotsa.h"
#include <WiFiUdp.h>

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

class IotsaNtpMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
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
