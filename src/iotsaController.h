#ifndef _IOTSACONTROLLER_H_
#define _IOTSACONTROLLER_H_
#include <stdint.h>
#include "iotsaConfig.h"       // iotsa_mode
#include "iotsaStatus.h"       // iotsaConfigSettingsWritable() reads iotsaStatus
#include "iotsaModeMachine.h"  // IotsaController::_modes
#include "iotsaRadioPolicy.h"  // IotsaController::_radio
#include "iotsaSleepPolicy.h"  // IotsaController::_sleep

// Intended to be included from iotsa.h

//
// IotsaController -- the device policy coordinator (cwi-dis/iotsa#106).
//
// A framework global (like iotsaConfig / iotsaStatus), begin()'d and tick()'d by
// IotsaApplication. It is a thin coordinator over three cohesive sub-policy
// objects held by value -- IotsaModeMachine, IotsaRadioPolicy, IotsaSleepPolicy --
// plus the deferred-reboot timer. Everything below is a one-line forward into a
// sub-policy; the public vocabulary (iotsaController.currentMode() etc.) is what
// the rest of the framework calls. See docs/controller-architecture.md.
//
class IotsaController {
public:
  void begin();  // from IotsaApplication::setup(), right after iotsaConfig.ensureConfigLoaded()
  void tick();   // from IotsaApplication::loop()

  // Deferred reboot: ESP.restart() after `ms`, long enough for the reply that
  // asked for it to flush first. Use the REBOOT_DELAY_* constants.
  void requestReboot(uint32_t ms);
  static constexpr uint32_t REBOOT_DELAY_HTTP_MS = 2000;  // after an HTTP/web response
  static constexpr uint32_t REBOOT_DELAY_BLE_MS  = 1000;  // after a BLE write-ack (cwi-dis/iotsa#130)

  // ---- radio-enablement policy (_radio) ----
  void setWifiRadioEnabled(bool on) { _radio.setWifiEnabled(on); }
  bool wifiRadioWanted() const { return _radio.wifiWanted(_modes.forcesWifiOn()); }
#ifdef IOTSA_WITH_BLE
  void setBleRadioEnabled(bool on) { _radio.setBleEnabled(on); }
  bool bleRadioWanted() const { return _radio.bleWanted(_modes.forcesBleOn()); }
#endif

  // ---- sleep/wake policy (_sleep) ----
  void noteActivity() { _sleep.noteActivity(); }
  void pauseSleep()  { _sleep.pauseSleep(); }
  void resumeSleep() { _sleep.resumeSleep(); }
  void postponeSleep(uint32_t ms) { _sleep.postponeSleep(ms); }
  bool canSleep() { return !_modes.forbidsSleep() && _sleep.canSleep(); }
  IotsaSleepPolicy& sleep() { return _sleep; }

  // ---- iotsa_mode state machine (_modes) ----
  void requestMode(iotsa_mode mode) { _modes.requestMode(mode); }
  void allowRequestedConfigurationMode() { _modes.allowRequestedConfigurationMode(); }
  void endConfigurationMode() { _modes.endConfigurationMode(); }
  // Activity happened: bump the sleep wake-window, and (if in a maintenance mode)
  // push its auto-expiry out. The two concerns are deliberately separate.
  void extendCurrentMode() {
    _sleep.noteActivity();
#ifndef ESP32
    ESP.wdtFeed();
#endif
    _modes.extendWindow();
  }
  void allowRCMDescription(const char *desc) { _modes.allowRCMDescription(desc); }
  const char *rcmInteractionDescription() const { return _modes.rcmInteractionDescription(); }
  void factoryReset() { _modes.factoryReset(); }
  // Pure predicate (cwi-dis/iotsa#106 step 5c -- no more `extend` side effect).
  // Callers that want "and keep the window open" call extendCurrentMode() after a
  // successful edit.
  bool inConfigurationMode() const { return _modes.inConfigurationMode(); }
  const char *modeName(iotsa_mode mode) const { return _modes.modeName(mode); }
  iotsa_mode currentMode() const { return _modes.currentMode(); }
  iotsa_mode requestedMode() const { return _modes.requestedMode(); }
  uint32_t currentModeEndTime() const { return _modes.currentModeEndTime(); }
  uint32_t requestedModeEndTime() const { return _modes.requestedModeEndTime(); }

  // Auto-expiry duration (seconds). A persisted, user-edited knob -- it lives on
  // iotsaConfig (config.cfg "rebootTimeout"); these are read/write forwarders.
  uint32_t modeTimeout() const { return iotsaConfig.configurationModeTimeout; }
  void setModeTimeout(uint32_t seconds) { iotsaConfig.configurationModeTimeout = seconds; }

  IotsaModeMachine& modeMachine() { return _modes; }
  IotsaRadioPolicy& radio() { return _radio; }

private:
  uint32_t _rebootAtMillis = 0;
  IotsaModeMachine _modes;
  IotsaRadioPolicy _radio;
  IotsaSleepPolicy _sleep;
};

extern IotsaController iotsaController;

// cwi-dis/iotsa#106 transitional: settings (WiFi credentials, hostname, ...) are
// writable in configuration mode, or on a not-yet-configured device serving its
// own AP. Replaces IotsaConfig::inConfigurationOrFactoryMode(); collapses to
// iotsaController.inConfigurationMode() once "no SSID => config mode" lands.
inline bool iotsaConfigSettingsWritable() {
  return iotsaController.inConfigurationMode()
      || (!iotsaStatus.wifiConfigured && iotsaStatus.wifiApActive);
}
#endif
