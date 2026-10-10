#include "iotsaRtc.h"
#include "iotsaConfigFile.h"

// How often to update the RTC from the system time (presumably ntp-synced)
#define UPDATE_INTERVAL 120000

#include <time.h>

// Seconds since the epoch for a broken-down UTC time -- what timegm() does, but
// neither the ESP8266 nor the ESP32 C library has timegm(), and mktime()
// interprets its argument as *local* time (cwi-dis/iotsa#104). Days-from-civil
// algorithm from Howard Hinnant's "chrono-Compatible Low-Level Date Algorithms".
static time_t utcTimeFromTm(const struct tm &tm) {
  int y = tm.tm_year + 1900;
  unsigned m = tm.tm_mon + 1;            // 1..12
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);                        // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + tm.tm_mday - 1;  // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
  const long days = (long)era * 146097 + (long)doe - 719468;
  return (time_t)days * 86400 + tm.tm_hour * 3600L + tm.tm_min * 60L + tm.tm_sec;
}

const char * IotsaRtcMod::isoTime()
{
  static char buf[32];
  _updateCurrentTime();
  sprintf(buf, "20%02d-%02d-%02dT%02d:%02d:%02dZ", 
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

void IotsaRtcMod::_updateCurrentTime() {
  uint32_t now = millis();
  if (currentTimeMillis == 0 || now < currentTimeMillis || now > currentTimeMillis + 1000) {
    currentTimeMillis = now;
    ds1302.getDateTime(&currentTime);
  }
}

#ifdef IOTSA_WITH_WEB
void
IotsaRtcMod::webHandler() {
  bool ok = true;
  if( api.webService->server->hasArg("isoTime")) {
    if (needsAuthentication("rtc")) return;
    ok = setIsoTime(api.webService->server->arg("isoTime"));
  }
  String message = "<html><head><title>Realtime Clock Settings</title></head><body><h1>Realtime Clock Settings</h1>";
  if (!ok) {
    message += "<p><em>Error setting RTC time</em></p>";
  }
  message += "<p>Current RTC time (UTC) is ";
  message += isoTime();
  message += "<form method='get'>Set RTC time: <input name='isoTime' value='";
  message += isoTime();
  message += "'><br>";
  message += "<input type='submit'></form>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaRtcMod::info() {
  String message = "<p>RTC time (UTC) is ";
  message += isoTime();

  message += ". See <a href=\"/rtcconfig\">/rtcconfig</a> to change time configuration.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaRtcMod::setup() {
  ds1302.init();
  _updateSysTime();
}

bool IotsaRtcMod::getHandler(const char *path, JsonObject& reply) {
  reply["isoTime"] = isoTime();
  return true;
}

bool IotsaRtcMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonObject reqObj = request.as<JsonObject>();
  const char *time = nullptr;
  if (getFromRequest<const char *>(reqObj, "isoTime", time)) {
    if (!setIsoTime(time)) return apiError(400, "isoTime: not an ISO 8601 time");
  }
  checkUnhandled(reqObj);
  return true;
}

void IotsaRtcMod::lateSetup() {
  api.setup("rtcconfig", true, true);
  name = "rtcconfig";
}

void IotsaRtcMod::configLoad() {
}

void IotsaRtcMod::configSave() {
}

void IotsaRtcMod::loop() {
  if (millis() > nextUpdateMillis) {
    _updateFromSysTime();
    nextUpdateMillis = millis() + UPDATE_INTERVAL;
  }
}

void IotsaRtcMod::_updateSysTime() {
  if (time(NULL) < 3600*24*366) {
    struct tm tm;
    memset((void *)&tm, 0, sizeof(tm));
    _updateCurrentTime();
    tm.tm_year = currentTime.year + 2000 - 1900;
    tm.tm_mon = currentTime.month-1;
    tm.tm_mday = currentTime.day;
    tm.tm_hour = currentTime.hour;
    tm.tm_min = currentTime.minute;
    tm.tm_sec = currentTime.second;
    time_t nowUtc = utcTimeFromTm(tm);
    struct timeval tv;
    tv.tv_sec = nowUtc;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    IotsaSerial.printf("RTC: Initialized system UTC time %d-%d-%d %d:%d:%d\n", 1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  }
}

void IotsaRtcMod::_updateFromSysTime() {
 char buf[64];
  time_t sysTime;
  time(&sysTime);
  struct tm *tp = gmtime(&sysTime);
  strftime(buf, sizeof(buf), "%FT%T", tp);
  setIsoTime(buf); 
  IotsaSerial.printf("RTC: Saved from system UTC time %s\n", buf); 
}
