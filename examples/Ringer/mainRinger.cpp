// A web interface to a buzzer. Has a web interface and a REST interface (and/or COAP interface, based on
// iotsa configuration options).
// The (user-centric) web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB
//
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"

#define PIN_ALARM 4 // GPIO4 connects to the buzzer

IotsaApplication application("Ringer Server");

// Configure modules we need
IotsaWifiMod wifiMod(application);  // wifi is always needed
IotsaOtaMod otaMod(application);    // we want OTA for updating the software (will not work with esp-201)

//
// Buzzer configuration and implementation
//
unsigned long alarmEndTime;

// Declaration of the Alarm module
class IotsaAlarmMod : public IotsaApiMod {
public:
  IotsaAlarmMod(IotsaApplication &_app) : IotsaApiMod(_app) {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  using IotsaBaseMod::needsAuthentication;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void handler();
};

//
// LCD configuration and implementation
//
void IotsaAlarmMod::setup() {
  pinMode(PIN_ALARM, OUTPUT); // Trick: we configure to input so we make the pin go Hi-Z.
}

#ifdef IOTSA_WITH_WEB
void IotsaAlarmMod::handler() {
  
  String msg;
  for (uint8_t i=0; i<server->args(); i++){
    if (server->argName(i) == "alarm") {
      const char *arg = server->arg(i).c_str();
      if (arg && *arg) {
        int dur = atoi(server->arg(i).c_str());
        if (dur) {
          alarmEndTime = millis() + dur*100;
          IotsaSerial.println("alarm on");
          digitalWrite(PIN_ALARM, HIGH);
        } else {
          alarmEndTime = 0;
        }
      }
    }
  }
  String message = "<html><head><title>Alarm Server</title></head><body><h1>Alarm Server</h1>";
  message += "<form method='get'>";
  message += "Alarm: <input name='alarm' value=''> (times 0.1 second)<br>\n";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
  
}

String IotsaAlarmMod::info() {
  return "<p>See <a href='/alarm'>/alarm</a> to use the buzzer. REST interface on <a href='/api/alarm'>/api/alarm</a>";
}
#endif // IOTSA_WITH_WEB

bool IotsaAlarmMod::getHandler(const char *path, JsonObject& reply) {
  int dur = 0;
  if (alarmEndTime) {
    dur = (alarmEndTime - millis())/100;
  }
  reply["alarm"] = dur;
  return true;
}

bool IotsaAlarmMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  int dur = 0;
  if (request.is<int>()) {
    dur = request.as<int>();
  } else if (request.is<JsonObject>()) {
    JsonObject reqObj = request.as<JsonObject>();
    dur = reqObj["alarm"];
  } else {
    return false;
  }
  if (dur) {
    alarmEndTime = millis() + dur*100;
    IotsaSerial.println("alarm on");
    digitalWrite(PIN_ALARM, HIGH);
  } else {
    alarmEndTime = 0;
  }
  return true;
}


void IotsaAlarmMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/alarm", std::bind(&IotsaAlarmMod::handler, this));
#endif
  api.setup("/api/alarm", true, true);
  name = "alarm";
}


void IotsaAlarmMod::loop() {
  if (alarmEndTime && millis() > alarmEndTime) {
    alarmEndTime = 0;
    IotsaSerial.println("alarm off");
    digitalWrite(PIN_ALARM, LOW);
  }
}

IotsaAlarmMod alarmMod(application);
//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.serverSetup();
}
 
void loop(void) {
  application.loop();
}
