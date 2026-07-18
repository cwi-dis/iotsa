# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [unreleased]

- Allow setting CPU speed to conserve battery (#98)
- Added workaround for some esp32c3 boards (#99)
- Show WiFi MAC address when in config or factory mode (#102)
- Fix `bleTargets` showing UUIDs instead of device names on macOS (#111)
- Document `--protocol hps` in help text
- Fix wificonfig reporting empty SSID when WiFi disabled on boot
- Fix allInfo crashing on 404 from hps module; hps now returns empty REST response
- Fix `iotsa networks` broken on macOS since airport tool removed; now uses CoreWLAN with wifi-cli fallback (#97)
- Add `backup` and `restore` commands to save/restore all module configs as JSON files
- Rework `dfu` as subcommands (`dfu info`, `dfu flash`, `dfu backup`, `dfu flashfs`, `dfu lsfs`, `dfu extractfs`, etc.); add partition-table awareness, LittleFS read support, and OTA slot management (`dfu otainfo`, `dfu otaset`, `dfu otaclear`)
- Fix ESP8266 builds broken by unguarded ESP32-only battery/WiFi calls (#135)
- Refuse `--ssid` WiFi switch when it would drop the only internet route, unless `--force` (#129)
- Fix `--ssid` reporting success before the WiFi join actually completes, and before confirming it's genuinely the requested config AP (#126)
- Add `--baud` option for DFU serial connections (some boards need a non-default rate)
- Promote `IotsaBLEClientMod`/`IotsaBLEClientConnection` from lissabon to core, with opt-in scan/advertise coordination against `IotsaBLEServerMod` (#138)
- Fix `IotsaBLEClientMod` never detecting natural scan-timeout completion (dead `scanComplete` callback didn't match the real `NimBLEScanCallbacks::onScanEnd` signature), which could leave scanning permanently wedged (#141)
- Fix BLE devices becoming unmatchable after NimBLE-Arduino 2.1.0 stopped advertising device names by default: advertise the name explicitly again, and fix `onResult`'s by-address fallback matching (unreachable + wrong iterator) plus wire up address-based reconnection from persisted config (#142)
- Fix a crash (`LoadProhibited`) from `onScanEnd()` racing with `loop()` over shared scanner state across FreeRTOS tasks, introduced by #141's fix
- Fix `IotsaBLEClientMod::canConnect()` unconditionally returning `true` since 2020, letting `connect()` be attempted while a scan is still active (reliably fails on real hardware) instead of waiting
- Redesign BLE client scanning: split discovery (hunting unknown/unaddressed devices) from presence-check (confirming known devices are still alive) scans, stop presence-check scans early once everyone's reconfirmed instead of always running the full duration, and make all the relevant durations/cooldowns configurable

## [2.8.1] - 2025-05-04

### Changed

- Version file was generated incorrectly. Fixed.

## [2.8] - 2025-05-03

### Changed

- Use platform espressif32@6.9
- Use platform espressif8266@4.0
- Use arduino framework v2 (known as framework-arduinoespressif32 v3.2 under platformio, sigh)
- Use esp32_idf_5_https_server and esp32_idf_5_https_server_compat
- Https key size upped to 2048 bits
- LittleFS is now the standard filesystem
- ArduinoJson version upped to 7.2
- Use arduino-cli for Arduino builds
- Use bleaktyped python package in stead of bleak

## [2.7] - 2023-06-03

### Added

- Added support for esp32c3
- Added support for HPS (HTTP Proxy Service) over BLE, which allows making API calls over Bluetooth.

### Changed

- Added support for esp32c3
- Added support for HPS (HTTP Proxy Service) over BLE, which allows making API calls over Bluetooth.
- Power management and enabling and disabling of wifi and ble redesigned
- Added more versioning information to /api/config

## [2.6] - 2022-10-07

### Added

- Added API for per-module objects.
- Added IotsaRtcMod to control ds1302 realtime clock module.

### Changed

- Various changes to configuration mode: can now be enabled (by the iotsa program) with other
  interaction than reboot. AP enabled while in configuration mode.
- iotsaBatteryMod sleep modes rationalized. Added various features for postponing sleep.
- Python iotsa package, including iotsa (formerly iotsaConfig) as a script entry point.
- Prefer libc timezone handling over Timezone library.
- Handle JSON buffer overflow, added jsonBufSize URL parameter.
- Moved to Arduino framework 3.0.
- Switched to ArduinoNimble for BLE support.
- Use ESP32Encoder library to handle rotary encoders in iotsaInput module.
- iotsa Python module and iotsaControl have support for BLE through commands bleTargets and ble.
- Python 2 support dropped, and type annotations added to Python module.
- LittleFS is now default filesystem (but SPIFFS can be selected with a define).

## [2.4.1] - 2020-04-23

### Changed

- Version number reported was wrong. Fixed.

## [2.4] - 2020-04-18

### Added

- Added BLE support (esp32 only): iotsa devices can now be a Bluetooth LE server.
- Added IotsaBatteryMod which enables power-saving for battery-operated devices.
- Added IotsaInput module for handling buttons, touchpads (esp32 only) and rotary encoders.

### Changed

- Fixed building of iotsa without WiFi support.
- Refactored various classes to enable wifi-less operation and different webserver implementations.
- Allow use of esp32_https_server_compat as http or https server (esp32 only). Enabled by default.
- WiFi can now switch between AP en STAtion mode on the fly, and is handled in loop() so
  boot is much faster.
- Versioning scheme changed: even minor numbers are stable, odd numbers are work-in-progress.

## [2.2] - 2019-12-03

### Changed

- Updated to PlatformIO 4
- newer version of esp8266 and esp32 Arduino frameworks
- new ArduinoJson API
- Rationalized PlatformIO/Arduino support
- Backups and file listing implemented on esp32

## [2.1] - 2019-06-02

### Changed

- Got rid of default password, it provides no extra security.
- Allow uploading of certificate/key as DER with POST requests.
- Write-only REST attributes now always have a "has_" boolean in the read interface to signal that they exist and are set.
- REST read interface was made more consistent.

## [2.0.1] - 2018-07-01

### Changed

- Various changes to make https (server side) more usable, such as
  interfacing to the Igor Certificate Authority, and forwarding http
  requests to https. Added some scripts to help create certificates.
- Bug fixes for some security issues (which allowed config changes when not in config mode)

## [2.0] - 2018-05-31

### Changed

- The API has changed in an incompatible way: you no longer need to create the IotsaWebServer object
  yourself. The IotsaApplication constructor does this, if needed.
- The server by-reference instance variable of application and module objects is now a pointer.
- There are a number of compile-time flags in iotsaBuildOptions.h that enable and disable
  various features such as REST api access, normal web access and more.
- COAP is supported (as an alternative to, or in addition to, REST access).
- HTTPS is supported, as an alternative to HTTP.

## [1.8.1] - 2018-05-18

### Changed

- Fixed issue with platformio not finding ESP8266HttpClient.h

## [1.8] - 2018-04-25

### Changed

- IotsaRequest and IotsaButton modules added.

## [1.7.2] - 2018-04-08

### Changed

- Enabled (experimental) esp32 support again.
- Various fixes to iotsaControl and how it interfaces to iotsa boards

## [1.7.1] - 2018-03-30

### Changed

- Added iotsaControl module and program to allow programmatic configuration of iotsa devices.
- Added more variables to /api/config for iotsaControl.
- Fixed case error in Esp.h include which stopped travis builds from working.
- Added iotsaVersion.h and return version info in config api.

## [1.6] - 2018-03-26

### Changed

- WiFi config and general config have been split into two modules (listening on /wifi and /config, and the
  respective /api endpoints). All general configuration parameters are now in a global iotsaConfig structure.
  The iotsaConfig module does not have to be instantiated, this happens automatically.
- Configuration mode can now be active on the normal WiFi network. The special private network is only used when
  no WiFi network is configured or the configured network is not available. Private network no longer implies
  configuration mode.
- All web form argument handling has been converted to no longer using looping over the arguments.
- Default timeouts for reprogramming and configuration mode and such set to 5 minutes.

## [1.5] - 2018-03-08

### Changed

- Added support for capability-based access to resources.

## [1.4] - 2018-03-06

### Changed

- Fixed serious issue in design of access control.

### Changed

## [1.2.1] - 2018-03-05

### Changed

- Fixed typo in ArduinoJson dependency

## [1.2.0] - 2018-02-28

### Changed

- Added unified REST access through IotsaApiMod.
- Adadpted IotsaAuthMode to allow fine-grained access control over API methods.

## [1.1.0] - 2018-01-29

### Changed

- Ported to platformIO (in addition to Arduino IDE)

## [1.0.1] - 2017-07-17

### Changed

- Added IotsaMod::htmlEncode() method to help escaping strings embedded in the HTML.
- Documentation updates

## [1.0] - 2017-04-01

### Changed

- Initial github release.
