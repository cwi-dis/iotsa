#include "iotsaNtp.h"
#include "iotsaConfigFile.h"
#include <time.h>
#include <stdlib.h>

unsigned long IotsaNtpMod::utcTime()
{
  return time(NULL);
}

int IotsaNtpMod::localSeconds()
{
  time_t sysTime;
  time(&sysTime);
  struct tm *tp = localtime(&sysTime);
  return tp->tm_sec;
}

int IotsaNtpMod::localMinutes()
{
  time_t sysTime;
  time(&sysTime);
  struct tm *tp = localtime(&sysTime);
  return tp->tm_min;
}

int IotsaNtpMod::localHours()
{
  time_t sysTime;
  time(&sysTime);
  struct tm *tp = localtime(&sysTime);
  return tp->tm_hour;
}

int IotsaNtpMod::localHours12()
{
  return localHours() % 12;
}

bool IotsaNtpMod::localIsPM()
{
  return localHours() >= 12;
}

String IotsaNtpMod::isoTime()
{
  char buf[64];
  time_t sysTime;
  time(&sysTime);
  struct tm *tp = localtime(&sysTime);
  strftime(buf, sizeof(buf), "%FT%T", tp);
  return String(buf);
}

#ifdef IOTSA_WITH_WEB
void
IotsaNtpMod::webHandler() {
  bool anyChanged = false;
  if( api.webService->server->hasArg("ntpServer")) {
    if (needsAuthentication("ntp")) return;
    ntpServer = api.webService->server->arg("ntpServer");
    anyChanged = true;
  }
  if (api.webService->server->hasArg("tzDescription")) {
    if (needsAuthentication("ntp")) return;
    tzDescription = api.webService->server->arg("tzDescription");
    anyChanged = true;
  }
  if (anyChanged) {
    parseTimezone(tzDescription);  // also restarts SNTP, so a new server takes effect now
    configSave();
  }
  
  String message = "<html><head><title>NTP Client Settings</title></head><body><h1>NTP Client Settings</h1>";
  message += "<p>Current UTC time is ";
  message += String(utcTime());
  message += ".<br>Current local time is ";
  message += String(localHours());
  message += ":";
  message += String(localMinutes());
  message += ":";
  message += String(localSeconds());
  message += " or ";
  message += isoTime();
  message += ".</p>";
  message += "<form method='get'>NTP server: <input name='ntpServer' value='";
  message += htmlEncode(ntpServer);
  message += "'><br>";
  message += "Timezone change information: <input name='tzDescription' value='";
  message += htmlEncode(tzDescription);
  message += "'><br>(format: unix TZ)<br>";
  message += "<input type='submit'></form>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaNtpMod::info() {
  String message = "<p>Local time is ";
  message += isoTime();
  message += ", timezone is ";
  message += tzDescription;
  message += ". ";
  message += "See <a href=\"/ntpconfig\">/ntpconfig</a> to change time configuration.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaNtpMod::setup() {
  configLoad();
}

bool IotsaNtpMod::getHandler(const char *path, JsonObject& reply) {
  reply["ntpServer"] = ntpServer;
  reply["tzDescription"] = tzDescription;
  return true;
}

bool IotsaNtpMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "ntpServer", ntpServer)) {
    anyChanged = true;
  }
  if (getFromRequest<const char *>(reqObj, "tzDescription", tzDescription)) {
    anyChanged = true;
  }
  if (anyChanged) {
    parseTimezone(tzDescription);  // also restarts SNTP, so a new server takes effect now
    configSave();
  }
  checkUnhandled(reqObj);
  return true;
}

void IotsaNtpMod::lateSetup() {
  api.setup("ntpconfig", true, true);
  name = "ntpconfig";
}

void IotsaNtpMod::configLoad() {
  IotsaConfigFileLoad cf("/config/ntp.cfg");
  cf.get("ntpServer", ntpServer, "pool.ntp.org");
  String newTzdesc;
  cf.get("tzDescription", newTzdesc, "0");
  parseTimezone(newTzdesc);
}

void IotsaNtpMod::configSave() {
  IotsaConfigFileSave cf("/config/ntp.cfg");
  cf.put("ntpServer", ntpServer);
  cf.put("tzDescription", tzDescription);
}

void IotsaNtpMod::loop() {
  // Nothing to do: the platform's SNTP client (started by parseTimezone())
  // runs on its own.
}

// Installs the timezone and (re)starts the platform SNTP client with the
// current server.
void IotsaNtpMod::parseTimezone(const String& newDesc) {
  tzDescription = newDesc;
#ifdef ESP8266
  configTime(newDesc.c_str(), ntpServer.c_str());
#else
  configTzTime(newDesc.c_str(), ntpServer.c_str());
#endif
}
