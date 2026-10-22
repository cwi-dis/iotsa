#include "iotsaNtp.h"
#include "iotsaConfigFile.h"
#include <time.h>
#include <stdlib.h>

void IotsaNtpMod::setup() {
  configLoad();
}

void IotsaNtpMod::lateSetup() {
  api.setup("ntpconfig", true, true);
  name = "ntpconfig";
}

void IotsaNtpMod::loop() {
  // Nothing to do: the platform's SNTP client (started by parseTimezone())
  // runs on its own.
}

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

bool IotsaNtpMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
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
    if (changed) *changed = true;
  }
  checkUnhandled(reqObj);
  return true;
}

bool IotsaNtpMod::getHandler(const char *path, JsonObject& reply) {
  reply["ntpServer"] = ntpServer;
  reply["tzDescription"] = tzDescription;
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaNtpMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.argsNeedRight("ntp");
  bool changed = ctx.arg("ntpServer", ntpServer, IotsaWebContext::Saved);
  changed |= ctx.arg("tzDescription", tzDescription, IotsaWebContext::Saved);
  if (changed) {
    parseTimezone(tzDescription);  // also restarts SNTP, so a new server takes effect now
  }
}

void IotsaNtpMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    ctx.showStatus("UTC time", "", String(utcTime()));
    ctx.showStatus("Local time", "", isoTime());
  } else {
    IotsaWebContext::Form form(ctx);
    ctx.showField("NTP server", "ntpServer", ntpServer, IotsaWebContext::Saved);
    ctx.showField("Timezone", "tzDescription", tzDescription, IotsaWebContext::Saved, "POSIX TZ, for example CET-1CEST,M3.5.0,M10.5.0/3");
  }
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
