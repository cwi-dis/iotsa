#include "iotsaRtc.h"
#include "iotsaConfigFile.h"

const char * IotsaRtcMod::isoTime()
{
  static char buf[32];
  _updateCurrentTime();
  sprintf(buf, "20%02d-%02d-%02dT%02d:%02d:%02d", 
    currentTime.year, 
    currentTime.month, 
    currentTime.day, 
    currentTime.hour, 
    currentTime.minute, 
    currentTime.second);
  return buf;
}

bool IotsaRtcMod::setIsoTime(const char *time)
{
  int year, month, day, hour, minute, second;
  int n = sscanf(time, "%d-%d-%dT%d:%d:%d",
    &year,
    &month,
    &day,
    &hour,
    &minute,
    &second);
  if (n != 6) return false;
  if (year > 2000) year -= 2000;
  currentTime.year = year;
  currentTime.month = month;
  currentTime.day = day;
  currentTime.hour = hour;
  currentTime.minute = minute;
  currentTime.second = second;
  ds1302.setDateTime(&currentTime);
  return true;
}

int IotsaRtcMod::localSeconds()
{
  _updateCurrentTime();
  return currentTime.second;
}

int IotsaRtcMod::localMinutes()
{
  _updateCurrentTime();
  return currentTime.minute;
}

int IotsaRtcMod::localHours()
{
  _updateCurrentTime();
  return currentTime.hour;
}

int IotsaRtcMod::localHours12()
{
  _updateCurrentTime();
  return currentTime.hour % 12;
}

bool IotsaRtcMod::localIsPM()
{
  return localHours() >= 12;
}

void IotsaRtcMod::_updateCurrentTime() {
  uint32_t now = millis();
  if (currentTimeMillis == 0 || now < currentTimeMillis || now > currentTimeMillis + 1000) {
    currentTimeMillis = now;
    ds1302.getDateTime(&currentTime);
  }
}

#ifdef IOTSA_WITH_WEB
void
IotsaRtcMod::handler() {
  bool ok = true;
  if( server->hasArg("isoTime")) {
    if (needsAuthentication("rtc")) return;
    ok = setIsoTime(server->arg("isoTime"));
  }
  String message = "<html><head><title>Realtime Clock Settings</title></head><body><h1>Realtime Clock Settings</h1>";
  if (!ok) {
    message += "<p><em>Error setting RTC time</em></p>";
  }
  message += "<p>Current RTC time is ";
  message += isoTime();
  message += "<form method='get'>Set RTC time: <input name='isoTime' value='";
  message += isoTime();
  message += "'><br>";
  message += "<input type='submit'></form>";
  server->send(200, "text/html", message);
}

String IotsaRtcMod::info() {
  String message = "<p>RTC time is ";
  message += isoTime();

  message += ". See <a href=\"/rtcconfig\">/rtcconfig</a> to change time configuration.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaRtcMod::setup() {
  ds1302.init();
}

#ifdef IOTSA_WITH_API
bool IotsaRtcMod::getHandler(const char *path, JsonObject& reply) {
  reply["isoTime"] = isoTime();
  return true;
}

bool IotsaRtcMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("isoTime")) {
    const char * time = reqObj["isoTime"].as<char *>();
    anyChanged = setIsoTime(time);
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaRtcMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/rtcconfig", std::bind(&IotsaRtcMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/rtcconfig", true, true);
  name = "rtcconfig";
#endif
}

void IotsaRtcMod::configLoad() {
}

void IotsaRtcMod::configSave() {
}

void IotsaRtcMod::loop() {
}
