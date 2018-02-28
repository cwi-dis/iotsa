#include "iotsaNtp.h"
#include "iotsaConfigFile.h"

#ifdef WITH_TIMEZONE_LIBRARY
#include <Timezone.h>
#endif

#define NTP_INTERVAL  600 // How often to ask for an NTP reading
#define NTP_MIN_INTERVAL 20 // How often to ask if we have no NTP reading yet

const unsigned int NTP_PORT = 123;

unsigned long IotsaNtpMod::utcTime()
{
  return millis() / 1000 + utcTimeAtMillisEpoch;
}

unsigned long IotsaNtpMod::localTime()
{
#ifdef WITH_TIMEZONE_LIBRARY
  if (tz) {
  	return tz->toLocal(utcTime());
  } else {
  	return utcTime();
  }
#else
  return utcTime() - minutesWestFromUtc*60;
#endif
}

int IotsaNtpMod::localSeconds()
{
  return localTime() % 60;
}

int IotsaNtpMod::localMinutes()
{
  return localTime() / 60 % 60;
}

int IotsaNtpMod::localHours()
{
  return localTime() / 3600 % 24;
}

int IotsaNtpMod::localHours12()
{
  return localTime() / 3600 % 12;
}

bool IotsaNtpMod::localIsPM()
{
  return localHours() >= 12;
}

void
IotsaNtpMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "ntpServer") {
    	if (needsAuthentication("ntp")) return;
    	ntpServer = server.arg(i);
    	anyChanged = true;
    }
#ifdef WITH_TIMEZONE_LIBRARY
	if (server.argName(i) == "tzDescription") {
		if (needsAuthentication("ntp")) return;
		parseTimezone(server.arg(i));
		anyChanged = true;
	}
#else
    if( server.argName(i) == "minutesWest") {
    	if (needsAuthentication("ntp")) return;
    	minutesWestFromUtc = server.arg(i).toInt();
    	anyChanged = true;
    }
#endif
    if (anyChanged) configSave();
  }
  String message = "<html><head><title>NTP Client Settings</title></head><body><h1>NTP Client Settings</h1>";
  message += "<p>Current UTC time is ";
  message += String(utcTime());
  message += ".<br>Current local time is ";
  message += String(localTime());
  message += " or ";
  message += String(localHours());
  message += ":";
  message += String(localMinutes());
  message += ":";
  message += String(localSeconds());
  message += ".</p>";
  message += "<form method='get'>NTP server: <input name='ntpServer' value='";
  message += htmlEncode(ntpServer);
  message += "'><br>";
#ifdef WITH_TIMEZONE_LIBRARY
  message += "Timezone change information: <input name='tzDescription' value='";
  message += htmlEncode(tzDescription);
  message += "'><br>(format  twice <i>name,week(last=0),dow(sun=1),month,hour(utc),delta</i> for example <kbd>CEDT,0,1,3,1,120,CET,0,1,10,1,60</kbd>)<br>";
#else
  message += "Minutes west from UTC: <input name='minutesWest' value='";
  message += String(minutesWestFromUtc);
  message += "'><br>";
#endif
  message += "<input type='submit'></form>";
  server.send(200, "text/html", message);
}

void IotsaNtpMod::setup() {
  nextNtpRequest = millis() + 1000; // Try after 1 second
  int ok = udp.begin(NTP_PORT);
  if (ok) {
    IotsaSerial.println("ntp: udp inited");
  } else {
    IotsaSerial.println("ntp: udp init failed");
  }
  configLoad();
}


bool IotsaNtpMod::getHandler(const char *path, JsonObject& reply) {
  reply["ntpServer"] = ntpServer;
#ifdef WITH_TIMEZONE_LIBRARY
  reply["tzDescription"] = tzDescription;
  long _minutesWest = utcTime() - localTime();
  reply["minutesWest"] = _minutesWest;
#else
  reply["minutesWest"] = minutesWest;
#endif
  return true;
}

bool IotsaNtpMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("ntpServer")) {
    ntpServer = reqObj.get<String>("ntpServer");
    anyChanged = true;
  }
#ifdef WITH_TIMEZONE_LIBRARY
  if (reqObj.containsKey("tzDescription")) {
    ntpServer = reqObj.get<String>("tzDescription");
    anyChanged = true;
  }
#else
  if (reqObj.containsKey("minutesWest")) {
    ntpServer = reqObj.get<int>("minutesWest");
    anyChanged = true;
  }
#endif
  if (anyChanged) configSave();
  return anyChanged;
}

void IotsaNtpMod::serverSetup() {
  server.on("/ntpconfig", std::bind(&IotsaNtpMod::handler, this));
  api.setup("/api/ntpconfig", true, true);
}

void IotsaNtpMod::configLoad() {
  IotsaConfigFileLoad cf("/config/ntp.cfg");
  cf.get("ntpServer", ntpServer, "pool.ntp.org");
#ifdef WITH_TIMEZONE_LIBRARY
  String newTzdesc;
  cf.get("tzDescription", newTzdesc, "0");
  parseTimezone(newTzdesc);
#else
  cf.get("minutesWest", minutesWestFromUtc, 0);
#endif
 
}

void IotsaNtpMod::configSave() {
  IotsaConfigFileSave cf("/config/ntp.cfg");
  cf.put("ntpServer", ntpServer);
#ifdef WITH_TIMEZONE_LIBRARY
  cf.put("tzDescription", tzDescription);
#else
  cf.put("minutesWest", minutesWestFromUtc);
#endif
}

void IotsaNtpMod::loop() {
  unsigned long now = millis();
  // Check for clock rollover
  if (now < lastMillis) {
      IotsaSerial.println("ntp: Clock rollover");
      nextNtpRequest = now;
  }
  lastMillis = now;
  
  // Check whether we have to send an NTP request
  if (now >= nextNtpRequest) {
    if (utcTimeAtMillisEpoch == 0) {
      nextNtpRequest = now + NTP_MIN_INTERVAL*1000;
    } else {
      nextNtpRequest = now + NTP_INTERVAL*1000;
    }
    IPAddress address;
    const char *host = ntpServer.c_str();
    if (host == NULL || *host == '\0') return;
    if (!WiFi.hostByName(host, address)) {
	  IotsaSerial.print("ntp: Lookup for "); IotsaSerial.print(host); IotsaSerial.println(" failed.");
	  nextNtpRequest = now + NTP_MIN_INTERVAL*1000;
	  return;
	}		
    IFDEBUG { IotsaSerial.print("ntp: Lookup for "); IotsaSerial.print(host); IotsaSerial.print(" returned "); IotsaSerial.println(address); }
    memset(ntpPacket, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    ntpPacket[0] = 0b11100011;   // LI, Version, Mode
    ntpPacket[1] = 0;     // Stratum, or type of clock
    ntpPacket[2] = 6;     // Polling Interval
    ntpPacket[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    ntpPacket[12]  = 49;
    ntpPacket[13]  = 0x4E;
    ntpPacket[14]  = 49;
    ntpPacket[15]  = 52;
  
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    if (!udp.beginPacket(address, 123)) { //NTP requests are to port 123
      IotsaSerial.println("ntp: Problem writing UDP packet (beginPacket)");
	  nextNtpRequest = now + NTP_MIN_INTERVAL*1000;
	  return;
    }
    if (!udp.write(ntpPacket, NTP_PACKET_SIZE)) {
      IotsaSerial.println("ntp: Problem writing UDP packet (write)");
	  nextNtpRequest = now + NTP_MIN_INTERVAL*1000;
	  return;
    }
    if (!udp.endPacket()) {
      IotsaSerial.println("ntp: Problem writing UDP packet (endPacket)");
	  nextNtpRequest = now + NTP_MIN_INTERVAL*1000;
	  return;
    }
    IFDEBUG IotsaSerial.println("ntp: Sent NTP packet");
  }

  // And check whether we have received an NTP packet
  int cb = udp.parsePacket();
  //int cb; while ((cb=udp.parsePacket()) == 0 && millis() < now+3000);
  if (cb == NTP_PACKET_SIZE) {
    // We've received a packet, read the data from it
    udp.read(ntpPacket, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(ntpPacket[40], ntpPacket[41]);
    unsigned long lowWord = word(ntpPacket[42], ntpPacket[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //IotsaSerial.print("Seconds since Jan 1 1900 = " );
    //IotsaSerial.println(secsSince1900);

    // now convert NTP time into everyday time:
    IotsaSerial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long nowUtc = secsSince1900 - seventyYears;
    utcTimeAtMillisEpoch = nowUtc - (millis() / 1000);
    IFDEBUG { IotsaSerial.print("ntp: Now(utc)="); IotsaSerial.print(utcTime()); IotsaSerial.print(" now(local)="); IotsaSerial.println(localTime()); }
  }
}

String IotsaNtpMod::info() {
  String message = "<p>Local time is ";
  message += String(localHours());
  message += ":";
  message += String(localMinutes());
  message += ":";
  message += String(localSeconds());
  message += ", timezone is ";
#ifdef WITH_TIMEZONE_LIBRARY
  message += tzDescription;
  message += ". ";
#else
  message += String(minutesWestFromUtc);
  message += " minutes west of Greenwich. ";
#endif
  message += "See <a href=\"/ntpconfig\">/ntpconfig</a> to change time configuration.</p>";
  return message;
}

#ifdef WITH_TIMEZONE_LIBRARY
void IotsaNtpMod::parseTimezone(const String& newDesc) {
	TimeChangeRule dstRule, stdRule;
	char descBuffer[80];
	char *token;
	
	tzDescription = String();
	if (tz) delete tz;
	
	// Special case: a single number means a constant timezone offset.
	if (strchr(newDesc.c_str(), ',') == NULL) {
		strncpy(dstRule.abbrev, newDesc.c_str(), 5); dstRule.abbrev[5] = '\0';
		dstRule.week = 1;
		dstRule.dow = 1;
		dstRule.month = 1;
		dstRule.hour = 0;
		dstRule.offset = newDesc.toInt();
		tzDescription = String(dstRule.offset);
		tz = new Timezone(dstRule, dstRule);
		return;
	}
	
	// Normal case: 12 fields separated by commas.
	strncpy(descBuffer, newDesc.c_str(), 80);
	token = strtok(descBuffer, ",");
	if (token == NULL) return;
	strncpy(dstRule.abbrev, token, 5); dstRule.abbrev[5] = '\0';
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	dstRule.week = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	dstRule.dow = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	dstRule.month = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	dstRule.hour = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	dstRule.offset = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	strncpy(stdRule.abbrev, token, 5); stdRule.abbrev[5] = '\0';
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	stdRule.week = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	stdRule.dow = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	stdRule.month = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	stdRule.hour = atoi(token);
	
	token = strtok(NULL, ",");
	if (token == NULL) return;
	stdRule.offset = atoi(token);
	
	tzDescription = newDesc;
	tz = new Timezone(dstRule, stdRule);
}
#endif // WITH_TIMEZONE_LIBRARY
