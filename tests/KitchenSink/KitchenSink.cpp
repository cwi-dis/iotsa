//
// KitchenSink: not a tutorial, a stress rig (see iotsa#151/#203). Instantiates as many
// iotsa modules together as is mutually sane, to measure worst-case flash/RAM
// consumption and to serve as a hardware test bed for module interaction bugs.
//
// This intentionally lives under tests/, not examples/: unlike every other sketch in
// this repo it isn't meant to be copied as a starting point for a real device.
//
// Deliberately excluded:
// - iotsaButton: superseded by iotsaInput (iotsa#160), and the two headers can't even
//   be included together (both define a top-level `class Button`, with different
//   bases -- a straight ODR collision, not just redundancy).
// - IotsaUserMod: IotsaMultiUserMod is used as the sole auth provider instead; the two
//   are mutually exclusive as documented in iotsa#151 (both derive IotsaAuthMod).
// - iotsaLedControlMod-style REST/web wrapper: that's app-specific UX layered on top
//   of iotsaLed in each example that needs it, not something iotsa itself ships.
//
// Build flags -DIOTSA_WITH_BLE -DIOTSA_WITH_COAP -DIOTSA_WITH_HTTPS (see
// tests/KitchenSink/iotsa-build.json) add BLE/CoAP/HTTPS coverage on top of the
// always-on HTTP/REST -- those axes are additive, not exclusive, so a single sketch
// can exercise all of them together.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaMultiUser.h"
#include "iotsaCapabilities.h"
#include "iotsaOta.h"
#include "iotsaBattery.h"
#include "iotsaFiles.h"
#include "iotsaFilesUpload.h"
#include "iotsaFilesBackup.h"
#include "iotsaNtp.h"
#include "iotsaRtc.h"
#include "iotsaLogger.h"
#include "iotsaLed.h"
#include "iotsaInput.h"
#include "iotsaSimple.h"
#include "iotsaNothing.h"
#include "iotsaBLEServer.h"
#include "iotsaBLEClient.h"

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 15 // Pulled down during boot on esp8266, can be used for led afterwards.
#endif

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

// Real-time clock pins (DS1302). Pin numbers aren't hardware-verified for every board
// here -- see examples/DateTime, which uses the same defaults across all its boards
// including nodemcuv2/esp32c3devkit -- this rig only cares that it compiles/links.
#define PIN_RTC_ENA 23
#define PIN_RTC_CLK 21
#define PIN_RTC_DAT 22

IotsaApplication application("Iotsa KitchenSink Test Rig");

// Multi-user auth, with capabilities chained on top. Both are exercised as the
// authProvider for every other module below.
IotsaMultiUserMod multiUserMod(application);
IotsaCapabilityMod capabilityMod(application, multiUserMod);
#define authProvider (&capabilityMod)

#ifdef IOTSA_WITH_WIFI
IotsaWifiMod wifiMod(application, authProvider);
#endif

IotsaOtaMod otaMod(application, authProvider);
IotsaBatteryMod batteryMod(application, authProvider);
IotsaFilesMod filesMod(application, authProvider);
IotsaFilesUploadMod filesUploadMod(application, authProvider);
IotsaFilesBackupMod filesBackupMod(application, authProvider);
IotsaNtpMod ntpMod(application, authProvider);
IotsaRtcMod rtcMod(application, PIN_RTC_ENA, PIN_RTC_CLK, PIN_RTC_DAT, authProvider);
IotsaLoggerMod loggerMod(application, authProvider);
IotsaLedMod ledMod(application, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800, (IotsaAuthMod *)&capabilityMod);
IotsaNothingMod nothingMod(application, authProvider);

// iotsaInput: a rotary encoder and a pushbutton (see examples/Input for pin meaning).
RotaryEncoder encoder(14, 2);
Button button(0, true, true, true);
Input* inputs[] = { &button, &encoder };
IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

// iotsaSimple: a trivial module with no state of its own, just to exercise the
// generic handler/info callback path.
static void kitchenSinkSimpleHandler() {
  application.server->send(200, "text/plain", "KitchenSink simple handler\n");
}
static String kitchenSinkSimpleInfo() {
  return "<p>See <a href=\"/kitchensink\">/kitchensink</a> (iotsaSimple test module).</p>";
}
IotsaSimpleMod simpleMod(application, "/kitchensink", kitchenSinkSimpleHandler, kitchenSinkSimpleInfo);

#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleServerMod(application);

// Subclassed only to start scanning from boot and to exercise coordinateWithServer
// (pauses/resumes bleServerMod's advertising around each scan) -- the same BLE
// server/client interplay flagged as unsolved in the #113 scoping discussion.
class KitchenSinkBLEClientMod : public IotsaBLEClientMod {
public:
  using IotsaBLEClientMod::IotsaBLEClientMod;
  void setup() override {
    IotsaBLEClientMod::setup();
    findUnknownDevices(true);
  }
};
KitchenSinkBLEClientMod bleClientMod(application);
#endif // IOTSA_WITH_BLE

void setup(void) {
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT);
#endif
#ifdef PIN_VUSB
  batteryMod.setPinVUSB(PIN_VUSB);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
#ifdef IOTSA_WITH_BLE
  IotsaBLEClientMod::coordinateWithServer = true;
#endif
  application.setup();
  application.serverSetup();
}

void loop(void) {
  application.loop();
}
