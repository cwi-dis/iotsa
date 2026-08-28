# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- BLE advertising could start before every module had registered its characteristics; CoAP/HPS companion modules depended on construction order to exist at all (#113)
- `IOTSA_WITHOUT_API` combined with auth (`IotsaUserMod`/`IotsaMultiUserMod`/`IotsaCapabilityMod`) failed to build (#206)
- `api.setup()` no longer registers a redundant, byte-identical web page per collection item (`IotsaButtonMod`/`IotsaMultiUserMod`/`IotsaUserMod`'s `name/N` sub-paths) (#217)
- `IOTSA_HAS_COAPSERVER` now actually respects `IOTSA_WITHOUT_API` (the guard was dead code); `IOTSA_HAS_HPSSERVER` now also requires `IOTSA_WITH_API`, matching what its own comment always claimed (#205)
- A REST-only, no-web-UI build (`IOTSA_WITH_API` on, `IOTSA_WITH_WEB` off) no longer silently loses `IotsaConfigMod`'s cert upload, `IotsaFilesUploadMod`, `IotsaFilesMod`, or `IotsaFilesBackupMod` -- the original motivating bug for #205, fixed alongside two more bugs it exposed once actually build-tested: `IotsaBaseModule::info()`/`htmlEncode()`/`percentDecode()` were needlessly `IOTSA_WITH_WEB`-gated (#206 already meant these to be unconditional), and `IotsaBLEClientMod::formHandler_fields()`/`formHandler_field_perdevice()` were declared unconditionally but only ever defined under `IOTSA_WITH_WEB`, a latent link error

### Changed

- Every module now always implements the REST/CoAP/HPS and BLE provider interfaces (harmless no-op defaults when unused), removing the `IotsaXxxModBaseMod` macro dance across 10 modules (#206)
- Renamed `IotsaMod`→`IotsaBaseModule`, `IotsaApiMod`→`IotsaModule`, merging the old `IotsaBaseMod`/`IotsaMod` split into one lenient base class (#206). **Breaking**: any app code deriving directly from `IotsaMod`/`IotsaApiMod` needs updating.
- `IotsaApiService` now chains its REST/CoAP/HPS transport services instead of composing them via nine duplicated `#ifdef` blocks (#213)
- `api.setup()` callers now pass a bare path segment instead of one already carrying the `/api/` prefix; each transport decides its own final path (#213). **Breaking**: any app module calling `api.setup("/api/...")` needs updating to drop the prefix.
- Web pages now register as a link in the same `IotsaApiService` transport chain as REST/CoAP/HPS -- a single `api.setup()` call also registers the module's page, instead of each module manually calling `server->on()`; module page logic moves from `handler()` to `webHandler()` (#213)
- CI now runs a minimal-coverage build (just `KitchenSink` + a handful of examples not otherwise covered) on `develop` pushes and PRs, reserving the full example/test matrix for `master`/tag pushes and PRs by default; opt into minimal on a PR with the `ci-minimal` label (#216)
- `IotsaBatteryMod`/`IotsaHpsServiceMod` now register their BLE characteristics from `lateSetup()` instead of `setup()`, consistent with how REST/CoAP/Web register (#210)
- Introduced an `IOTSA_HAS_xxx` naming family for build-flag facts computed from other flags, as opposed to `IOTSA_WITH_xxx`/`IOTSA_WITHOUT_xxx`, which are the only macros a build config ever sets directly: `IOTSA_WITH_HTTP_OR_HTTPS`→`IOTSA_HAS_WEBSERVER`, `IOTSA_WITH_REST`→`IOTSA_HAS_RESTSERVER`, `IOTSA_WITH_HPS`→`IOTSA_HAS_HPSSERVER`, plus new `IOTSA_HAS_COAPSERVER`/`IOTSA_HAS_FORWARDING_WEBSERVER` (#205). Purely internal to iotsa's own source -- none of these were ever set directly by a build config, so no app repo should be affected.
- Same for chip/hardware capability facts, under a module-scoped `IOTSA_<MODULE>_CAN_xxx` family instead: `IotsaInputMod`'s `IOTSA_WITH_TOUCH_SUPPORT`/`WAKEUP_SUPPORT`/`ESP32ENCODER_LIB`→`IOTSA_INPUT_CAN_TOUCH`/`CAN_WAKEUP`/`CAN_ENCODER_LIB`, plus a newly-named `IOTSA_BATTERY_CAN_RTC_MEM_POWER_DOMAINS` for a previously-inline chip-exclusion check in `IotsaBatteryMod`'s hibernate sleep (#205). **Breaking in principle** (public macro names any app could in theory have tested), though a check against lissabon found no actual usage.
- Removed `IOTSA_WITH_LEGACY_SPIFFS`, dead code since nothing ever defined it -- LittleFS is now unconditionally the only filesystem (#205)
- `IotsaBLEClientMod`'s `DEBUG_PRINT_ALL_CLIENTS` debug flag renamed to `IOTSA_DEBUG_BLE_PRINT_ALL_CLIENTS` -- a core-framework flag with no `IOTSA_` namespacing, real collision risk (#205)
- `NEOPIXEL_PIN`/`PIN_VBAT`/`PIN_VUSB`/`PIN_DISABLESLEEP` (shared across `Led`/`BLELed`/`Button`/`KitchenSink`) renamed to `IOTSA_PIN_NEOPIXEL`/`IOTSA_PIN_VBAT`/`IOTSA_PIN_VUSB`/`IOTSA_PIN_DISABLESLEEP`; `WITHOUT_VOLTAGE` removed in favor of gating `IOTSA_PIN_VBAT`/`IOTSA_PIN_VUSB` directly on `CONFIG_IDF_TARGET_ESP32C3`/`ESP32S3` (#205)
- Removed `IOTSA_WEBSERVER` (a debug-only string identifying which underlying WebServer implementation got selected -- useful during past implementation migrations, not since) and `IOTSA_WITH_SETRSACERT` (inlined at its one use site as the `!ESP32 && IOTSA_WITH_HTTPS` condition it was already equivalent to) (#205)
- `iotsaBuildOptions.h` restructured into 6 explicit stages (plain-value defaults, default-on `WITH`, default-off `WITH` listed, sanity checks on raw input, derived `HAS_` values, sanity checks on derived values) instead of one undifferentiated pile (#205)
- The HTTP(S) web server is now a peer service module (`IotsaHttpServiceMod`) instead of infrastructure privileged via `IotsaApplication` inheritance, the same tier as the CoAP/HPS companion mods; `IotsaApiServiceWeb`/`IotsaApiServiceRest` now share one server instance through it instead of each holding their own copy (#207)
- Renamed `serverSetup()`→`lateSetup()` across the framework, and removed `IotsaBaseModule`'s per-module `server` field -- API-having modules reach the shared server through their own `IotsaApiServiceWeb` link, everyone else through `IotsaApplication::server` (#211). **Breaking**: any app module overriding `serverSetup()` needs renaming to `lateSetup()`; a module reading `this->server` directly needs updating to one of the above.

### Removed

- `IotsaRestApiMod`/`IotsaCoapApiMod`, unused example/template classes (every handler a no-op `return false`) never instantiated anywhere in iotsa or any sibling repo -- the underlying `IotsaApiServiceRest`/`IotsaApiServiceCoap` they wrapped are still exercised by every real `IotsaModule`-derived module, so nothing loses coverage (#222)

### Changed

- `KitchenSink`'s rotary encoder/pushbutton pins are now `IOTSA_PIN_ENCODER_A`/`IOTSA_PIN_ENCODER_B`/`IOTSA_PIN_BUTTON` defines instead of hardwired constants; the button now fires an `IotsaRequest` self-loopback GET against `/api/nothing` on press, the only exercise of `IotsaRequest` anywhere in `examples/`/`tests/` (#222)
- New `sandbox/` toplevel, for sketches under active development that build on every push but aren't held to `examples/`'s doc-grade/full-board-coverage bar; `BLEClient` is the first tenant, moved out of `examples/` (#222)

## [2.9.2] - 2026-08-22

### Added

- ESP32-S3 SuperMini board support (`esp32s3supermini`), covered by CI (#194)

### Fixed

- `gen_build_matrix.py`: an example's own `build_flags` silently overwrote its board's inherited ones instead of merging (#194)
- `IotsaBatteryMod` hibernate sleep failed to build on ESP32-S3/-C3 (Arduino IDE only) -- those chips lack the RTC slow/fast memory power domains it unconditionally referenced (#194)
- `esp32s3supermini` Arduino IDE builds could exceed the default 1.2MB partition scheme (e.g. the Button example); 14 examples were missing the same `min_spiffs.csv` override their `esp32thing` variant already needed (#194)
- `IOTSA_WITH_BLE` combined with `IOTSA_WITHOUT_WEB` failed to build: `IotsaBLEClientMod`/`IotsaBLEServerMod`/`IotsaHpsServiceMod` declared web-only members without the `IOTSA_WITH_WEB` guard their base class uses, and the API-support consistency check didn't recognize HPS as a valid transport alongside REST/COAP (#194)
- Fix NTP time never syncing on ESP32 (timezone was set but SNTP was never actually started)
- `IotsaRequest::send()` can now return the HTTP response body via an optional out-param (#193)
- `IotsaRequest::send()` HTTPS on ESP8266 now probes for a small MFLN buffer size before connecting, instead of always allocating BearSSL's 16KB default -- was OOM-crashing the device outright on real-world heap budgets (#198)
- `IotsaRequest::send()` HTTPS on ESP8266 now pins the issuing root CA (`sslInfo`) like ESP32, instead of the exact leaf certificate's fingerprint -- the fingerprint broke on every certificate renewal, not just a CA change

## [2.9.1] - 2026-08-14

### Changed

- Version number in `library.json`/`library.properties` was not bumped for 2.9. Fixed.

## [2.9] - 2026-08-14

## added

- Added FileShare example: upload a file over HTTP and fetch it back (#156)
- Added BLEClient module (#145)
- iotsa command line tool commands added: dfu, backup, restore)
- Requests/replies larger than 512 bytes with HPS over BLE (#139)

## changed

- Fix ESP8266 builds broken by unguarded ESP32-only battery/WiFi calls (#135)
- Many BLE-related fixes/changes/extensions (#146, #147, #165, #130, #171, #172, #164)
- Dependencies updated and fixed (#149)
- Rationalized building with iotsa-config.json (#156)
- fixes to iotsa command line tool, Python package (#126, #129, #97, #111, #103)
- fixes for esp32c3 (#99)
- version strings unified (#169)
- Various REST structures rationalized (#173, #164)

## removed

- ESP8266 HTTPS support (#159)
- BLE support without NimBLE (#150)

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
