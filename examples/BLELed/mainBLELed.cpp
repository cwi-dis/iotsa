//
// A "Led" server, which allows control over a single NeoPixel (color,
// duration, on/off pattern). The led can be controlled through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//
// This is the application that is usually shipped with new iotsa boards.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"

// CHANGE: Add application includes and declarations here

#undef WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_BATTERY
#ifdef ESP32
#ifndef WITHOUT_VOLTAGE
#define PIN_VBAT 36 // Undefine to disable battery voltage measurements. Use 1:1 voltage divider.
#define PIN_VUSB 37 // Undefine to disable USB voltage measurements. Use 1:1 voltage divider.
#endif
#ifdef ESP32C3
#define PIN_DISABLESLEEP 9 // Define as pin to disable sleep (active low to disable)
#else
#define PIN_DISABLESLEEP 0 // Define as pin to disable sleep (active low to disable)
#endif
#endif // ESP32

IotsaApplication application("Iotsa BLE LED Server");
#ifdef IOTSA_WITH_WIFI
IotsaWifiMod wifiMod(application);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#ifdef WITH_BATTERY
#include "iotsaBattery.h"
IotsaBatteryMod batteryMod(application);
#endif

#include "iotsaBLEServer.h"
#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
#endif
//
// LED module. 
//

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 15 // Pulled down during boot on esp8266, can be used for led afterwards.
#endif

class IotsaLedControlMod : public IotsaLedMod, public IotsaBLEApiProvider {
public:
  using IotsaLedMod::IotsaLedMod;
  void setup() override;
  void serverSetup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
  void handler();

#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "3B000001-1226-4A53-9D24-AFA50C0163A3";
  static constexpr UUIDstring rgbUUID = "3B000002-1226-4A53-9D24-AFA50C0163A3";
#endif // IOTSA_WITH_BLE

};

#ifdef IOTSA_WITH_BLE
bool IotsaLedControlMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      uint32_t _rgb = bleApi.getAsInt(rgbUUID);
      set(_rgb, 1000, 0, 0x7fff);
      return true;
  }
  IotsaSerial.println("ledControlMod: ble: write unknown uuid");
  return false;
}

bool IotsaLedControlMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == rgbUUID) {
      bleApi.set(rgbUUID, rgb);
      return true;
  }
  IotsaSerial.println("ledControlMod: ble: read unknown uuid");
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_WITH_WEB
void
IotsaLedControlMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  bool anyChanged = false;
  uint32_t _rgb = 0xffffff;
  if( server->hasArg("rgb")) {
    _rgb = strtol(server->arg("rgb").c_str(), 0, 16);
    anyChanged = true;
  }
  if (anyChanged) set(_rgb, 1000, 0, 0x7fff);
  
  String message = "<html><head><title>Led Server</title></head><body><h1>Led Server</h1>";
  message += "<form method='get'>";
  message += "Color (hex rrggbb): <input type='text' name='rgb' value='" + String(rgb, HEX) + "'><br>";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedControlMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/led\">/led</a> for setting the LED color.";
#ifdef IOTSA_WITH_REST
  rv += " Or use REST api at <a href='/api/led'>/api/led</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaLedControlMod::getHandler(const char *path, JsonObject& reply) {
  reply["rgb"] = rgb;
  return true;
}

bool IotsaLedControlMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  uint32_t _rgb = request["rgb"]|0xffffff;
  set(_rgb, 1000, 0, 0x7fff);
  return true;
}
#endif // IOTSA_WITH_API

void IotsaLedControlMod::serverSetup() {
  name = "led";
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/led", std::bind(&IotsaLedControlMod::handler, this));
#endif // IOTSA_WITH_WEB
#ifdef IOTSA_WITH_API
  api.setup("/api/led", true, true);
#endif
}

void IotsaLedControlMod::setup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(rgbUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UINT32, 0x2700, "RGBx color");
#endif
}

IotsaLedControlMod ledMod(application, NEOPIXEL_PIN);

// Standard setup() method, hands off most work to the application framework
void setup(void){
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT);
#endif
#ifdef PIN_VUSB
  batteryMod.setPinVUSB(PIN_VUSB);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  // As an example, we allow switching to configuration mode by sending a BLE command
  batteryMod.allowBLEConfigModeSwitch();
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

