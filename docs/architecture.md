# iotsa architecture: the parts and their layers

What iotsa is made of, from the most fundamental part to the most optional, as of
iotsa 3.0. For writing an application you don't need this: see the README's
[module categories](../README.md#module-categories). This is for working on iotsa
itself.

The layers come from the design discussion in
[#204](https://github.com/cwi-dis/iotsa/issues/204) (the reasons) and
[#209](https://github.com/cwi-dis/iotsa/issues/209) (the target). Where the code doesn't
match the target yet, the layer says so.

## Terms

- **Module**: a class derived from `IotsaBaseModule`, registered with the application,
  with `setup()`, `lateSetup()` and `loop()`. Most derive from `IotsaModule`, which adds
  the API and the web page.
- **Automatic module**: a module that `IotsaApplication::setup()` creates itself, through
  `ensure()`, unless the program declared one.
- **Provider interface**: what a module implements to be seen from outside: settings in
  a config file, a web page, API handlers, BLE characteristics.
- **Transport**: a way into the device for an API request: REST, web, CoAP, HPS (the API
  over BLE), or a module's own BLE characteristic.
- **Building block**: a class that modules use, but that isn't a module itself.

## The layers

1. **Substrate**: the mechanism that makes modules work. `IotsaApplication`,
   `IotsaBaseModule`, module registration (early modules first), and the calls to each
   module's `setup()`, `lateSetup()` and `loop()`. `postToLoop()` and `runInLoop()` run
   work in the loop task, which is where every handler runs
   ([#236](https://github.com/cwi-dis/iotsa/issues/236)). In `iotsa.h`/`iotsa.cpp`.

2. **Provider interfaces**: on every module, each with defaults that do nothing
   ([#206](https://github.com/cwi-dis/iotsa/issues/206)). `configLoad()`/`configSave()`
   for settings, `formHandler_args()`/`formHandler_fields()` and `info()` for the web
   page, `IotsaApiProvider` (`getHandler()`/`putHandler()`/`postHandler()`) for REST,
   CoAP and HPS, `IotsaBLEProvider` (`bleGetHandler()`/`blePutHandler()`) for a module's
   own BLE service. `IotsaBaseModule` derives from both providers.

3. **Foundation**: what `IotsaApplication::setup()` does before any module: the serial
   port, mounting (and if needed formatting) the LittleFS filesystem, and loading
   `iotsaConfig` (hostname, mode timeout, HTTPS certificate). `iotsaConfigFile.h` is the
   file format every module's `configLoad()`/`configSave()` uses.

4. **Controllers and status**: `iotsaController` decides the mode (normal,
   configuration, OTA, factory reset), sleep, which radios may be on, reboots, and owns
   the watchdog ([#244](https://github.com/cwi-dis/iotsa/issues/244)). `iotsaStatus` is
   where the controllers publish state for everyone to read, including the status LED's
   colour ([#176](https://github.com/cwi-dis/iotsa/issues/176)). `IotsaWifiController`
   and `IotsaWifiDriver` are WiFi's own policy and mechanism, owned by `IotsaWifiMod`.
   See [controller-architecture.md](controller-architecture.md) and
   [wifi-controller-design.md](wifi-controller-design.md).

5. **Core modules**, automatic: configuration (`IotsaConfigMod`, also without WiFi,
   [#195](https://github.com/cwi-dis/iotsa/issues/195)), run mode (`IotsaRunmodeMod`, the
   way in to the controller from outside, and the sleep executor), WiFi (`IotsaWifiMod`).

6. **Inbound transports**, automatic when built in: the web server
   (`IotsaHttpServiceMod`, a module like the others since
   [#207](https://github.com/cwi-dis/iotsa/issues/207)) with REST (`iotsaApiRest`) and
   the web pages (`iotsaApiWeb`, `IotsaWebContext`); CoAP; the BLE server
   (`IotsaBLEServerMod`) with HPS. Each request goes through an `IotsaApiContext` or
   `IotsaWebContext`, which checks rights and logs one result line
   ([#296](https://github.com/cwi-dis/iotsa/issues/296),
   [#182](https://github.com/cwi-dis/iotsa/issues/182)).

7. **Access control**: `IotsaAuthMod`, with `IotsaUserMod` and `IotsaCapabilityMod`.
   Optional: without an authenticator everything is allowed. Capabilities don't work in
   3.0 ([#304](https://github.com/cwi-dis/iotsa/issues/304)); the permission model is
   redesigned in v4.0 ([#107](https://github.com/cwi-dis/iotsa/issues/107)). The target,
   an authenticator that is always there and allows everything by default, is not done.

8. **Time**: `IotsaNtpMod`, `IotsaRtcMod`, in either order
   ([#104](https://github.com/cwi-dis/iotsa/issues/104)). Optional. The target, a time
   source that is always there (falling back to `millis()`), is not done.

9. **Maintenance**: OTA (`IotsaOtaMod`, automatic with WiFi), and optional: the logger,
   files, upload and backup. The file modules are web-only, with no API.

10. **Outbound commands**: this device as a client. `IotsaRequest` (http(s), a building
    block used by `IotsaButtonMod` and by application modules), and the BLE client
    (`IotsaBLEClientMod`, with `IotsaBLEClientManagerMod` for a list of devices users
    can change). The BLE client has no generic command layer like `IotsaRequest` has
    ([#208](https://github.com/cwi-dis/iotsa/issues/208)).

11. **Common hardware**: the status LED (`IotsaLedMod`), automatic when the board
    definition has one ([#272](https://github.com/cwi-dis/iotsa/issues/272)); the battery
    and USB voltages (`IotsaBatteryMod`), optional
    ([#106](https://github.com/cwi-dis/iotsa/issues/106) moved its sleep code to the run
    mode module and the controller).

12. **Input hardware**: `IotsaInputMod` (buttons, rotary encoders, touchpads), and
    `IotsaButtonMod`, deprecated, to be merged into it
    ([#160](https://github.com/cwi-dis/iotsa/issues/160)). Optional.

13. **Scaffolding**: `IotsaSimpleMod` (a module from two functions) and `IotsaNothingMod`
    (a test fixture).

The building blocks sit beside the layers: `IotsaApiContext` and `IotsaWebContext`
(`iotsaContext.h`, `iotsaWebContext.h`), `IotsaCollection` and `IotsaVector<T>`
([#309](https://github.com/cwi-dis/iotsa/issues/309)), `iotsaConfigFile.h`,
`iotsaLog.h`, `IotsaDeadline` ([#235](https://github.com/cwi-dis/iotsa/issues/235)),
`IotsaRequest`.
