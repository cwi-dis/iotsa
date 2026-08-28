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

#ifndef IOTSA_PIN_NEOPIXEL
#define IOTSA_PIN_NEOPIXEL 15 // Pulled down during boot on esp8266, can be used for led afterwards.
#endif

#ifndef IOTSA_PIN_BUTTON
#define IOTSA_PIN_BUTTON 0 // GPIO0 is the "Boot" pin, wired to a pushbutton on most dev boards.
#endif
#ifndef IOTSA_PIN_ENCODER_A
#define IOTSA_PIN_ENCODER_A 14
#endif
#ifndef IOTSA_PIN_ENCODER_B
#define IOTSA_PIN_ENCODER_B 2
#endif

#ifdef ESP32
// ESP32-C3 and -S3 boards in this family don't have the 1:1 voltage-divider wiring
// on these pins -- gate on the chip target directly (CONFIG_IDF_TARGET_xxx, the
// ESP-IDF-provided macro, correct under both PlatformIO and Arduino IDE/arduino-cli)
// rather than via a separate opt-out build flag.
#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#define IOTSA_PIN_VBAT 36 // Undefine to disable battery voltage measurements. Use 1:1 voltage divider.
#define IOTSA_PIN_VUSB 37 // Undefine to disable USB voltage measurements. Use 1:1 voltage divider.
#endif
#ifdef CONFIG_IDF_TARGET_ESP32C3
#define IOTSA_PIN_DISABLESLEEP 9 // Define as pin to disable sleep (active low to disable)
#else
#define IOTSA_PIN_DISABLESLEEP 0 // Define as pin to disable sleep (active low to disable)
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
IotsaLedMod ledMod(application, IOTSA_PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800, (IotsaAuthMod *)&capabilityMod);
IotsaNothingMod nothingMod(application, authProvider);

// iotsaInput: a rotary encoder and a pushbutton (see examples/Input for pin meaning).
RotaryEncoder encoder(IOTSA_PIN_ENCODER_A, IOTSA_PIN_ENCODER_B);
Button button(IOTSA_PIN_BUTTON, true, true, true);
Input* inputs[] = { &button, &encoder };
IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

// IotsaRequest: nothing else in examples/ or tests/ instantiates it (IotsaButtonMod,
// its only other user, is excluded above) -- wire it to the button so pressing it
// exercises the outbound-request code path, self-loopback GET against nothingMod's
// own REST endpoint so it needs no external server to hit (see cwi-dis/iotsa#222).
static IotsaRequest buttonRequest;
static bool kitchenSinkButtonPressed() {
  buttonRequest.url = "http://" + WiFi.localIP().toString() + "/api/nothing";
  return buttonRequest.send();
}

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
#ifdef IOTSA_PIN_VBAT
  batteryMod.setPinVBat(IOTSA_PIN_VBAT);
#endif
#ifdef IOTSA_PIN_VUSB
  batteryMod.setPinVUSB(IOTSA_PIN_VUSB);
#endif
#ifdef IOTSA_PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(IOTSA_PIN_DISABLESLEEP);
#endif
#ifdef IOTSA_WITH_BLE
  IotsaBLEClientMod::coordinateWithServer = true;
#endif
  button.setCallback(kitchenSinkButtonPressed);
  application.setup();
  application.lateSetup();
}

void loop(void) {
  application.loop();
}
