# Controller architecture (the `IotsaConfig` de-tangle)

**Status:** design, not yet implemented. Branch `106-runmode-module`. Part of
[cwi-dis/iotsa#106](https://github.com/cwi-dis/iotsa/issues/106), sibling to
[`wifi-controller-design.md`](wifi-controller-design.md).

`wifi-controller-design.md` said the WiFi machine gets designed first and "the
controller's shape then informs the `IotsaRunmodeMod` / `IotsaConfigMod` split rather
than the split being guessed at." This doc is that split, now that `IotsaWifiController`
exists and has been bench-proven.

## Root diagnosis

`IotsaConfig` (the `iotsaConfig` global) is a god-object spanning four unrelated
concerns:

1. **Identity + persisted settings** -- `hostName`, HTTPS cert/key, `configLoad/Save()`.
2. **The `iotsa_mode` state machine** -- `configurationMode`, `nextConfigurationMode`,
   the request + anti-tamper (hardware-reset) gate, timeouts, `begin/endConfigurationMode()`,
   `allowRequestedConfigurationMode()`, `extendCurrentMode()`, `factoryReset()`.
3. **Runmode hooks** -- `wifiDisabledOnBoot`, `bleDisabledOnBoot`,
   `pauseSleep()`/`resumeSleep()`/`postponeSleep()`/`canSleep()`, `postponeSleepMillis`,
   `activityExtraWakeDuration`, `extendCurrentModeCallback`, `rebootAtMillis` /
   `requestReboot()` + the reboot check in `loop()`.
4. **A read-mostly status board** -- `wifiEnabled`, `wifiStationConnected`,
   `wifiApActive`, `mdnsEnabled`, `networkIsUp()`, plus `getStatusColor()`,
   `getBootReason()`, `printHeapSpace()`.

Concerns 2 and 3 are actually *one* concern (see "The `IotsaController`" below). The
sleep/wake logic that pairs with concern 3 currently lives in `IotsaBatteryMod`, reached
from `IotsaConfig` through the `extendCurrentModeCallback` -- exactly the kind of
cross-object callback #106 exists to delete.

## Target: four framework globals

| global | role | write pattern |
|---|---|---|
| `iotsaConfig` | **identity** -- `hostName`, cert, `configLoad/Save()`, plus the `iotsa_mode` predicates (`inConfigurationMode()` etc.) | rare, only in a maintenance window, by the config UI |
| `iotsaStatus` | **status bus** -- `wifiStationConnected`, `wifiApActive`, `wifiEnabled`, `mdnsEnabled`, `currentMode`, `networkIsUp()`, `getBootReason()`, `getStatusColor()` | every tick, by the controllers |
| `iotsaController` | **policy coordinator** -- see below | -- |
| `app` | module registry + setup/loop | sketch-declared, not framework-declared |

`iotsaConfig` keeps its name and symbol: `iotsaConfig.hostName` and
`iotsaConfig.inConfigurationMode()` are referenced in every downstream repo and are the
bulk of all call sites. "Config" now means *the things a user configures* (identity +
the maintenance-mode machine's public face); the volatile runtime observations move to
`iotsaStatus`.

Identity vs status split on **opposite lifecycles**, which is why it is a real seam:

|  | identity (`iotsaConfig`) | status (`iotsaStatus`) |
|---|---|---|
| written | rarely, in a maintenance window | every tick, by controllers |
| persisted | yes | no |
| role | *input* to behaviour | *output* of behaviour |

`iotsaController` is a framework global for the same reason `iotsaConfig` is: core code
must reach it from everywhere, and it cannot rely on `app` (sketch-declared). "Owned by
`IotsaApplication`" only means `IotsaApplication::loop()` calls `iotsaController.tick()`,
exactly as it calls `iotsaConfig.loop()` today.

`iotsaStatus` is a plain data struct: no module, no `loop()`, no lifecycle. *If* it ever
grows a read-only external API it does not get its own module -- it is handled the way
`version` is: a `/api/status` path dispatched inside an existing module's `getHandler`,
registered read-only (`api.setup("status", true)`) and listed as a name in `/api/config`,
with no `IotsaStatusMod` class. Whether that guest handler lives in `IotsaConfigMod` or
`IotsaRunmodeMod` does not matter much.

## The `IotsaController`

One object, because its parts need each other's state synchronously within a single
tick to decide correctly:

- the **`iotsa_mode` machine** -- transitions, the anti-tamper "did we come from a
  hardware reset" gate, the timeout, and the *effects* of a mode (CONFIG/OTA => radios
  up, no sleep, config AP up; on expiry => sleep allowed again, AP dropped).
- **radio-enablement policy** -- which of {WiFi, BLE} may be powered: boot flags
  (`wifiDisabledOnBoot`, `bleDisabledOnBoot`), runtime toggles, and the current mode.
- **sleep/wake policy** -- the sleep decision, `postponeSleep()` / `pauseSleep()` from
  app modules, the WiFi<->sleep coupling (`disableSleepOnWiFi`, `disableWiFiOnSleep`),
  wake-window timing. Battery *hardware* sensing stays in `IotsaBatteryMod` and feeds
  settings in.
- **reboot scheduling** -- `requestReboot()`, and the fact that a requested mode
  activates only on the next boot.

`tick()` runs these in a defined order (mode -> radio -> sleep -> reboot) and publishes
the results into `iotsaStatus`. It is a **coordinator composed of cohesive sub-policies**
(`_modeMachine`, `_radioPolicy`, `_sleepPolicy`), one object because they cannot be
cleanly separated -- explicitly *not* "where leftover stuff goes."

Subordinate specialists it drives, does not absorb:

- **`IotsaWifiController`** -- unchanged role. `IotsaController` hands it "radio may be
  on" + "stay-calm / hunt-ok"; it runs its own STA/AP duty cycle inside that frame.
- **BLE server enable** -- same, driven from the radio-enablement policy.

## `iotsa_mode`: implementation here, API in `IotsaConfigMod`

Same split as WiFi (`IotsaWifiMod` keeps `/api/wificonfig`, `IotsaWifiController` owns
the machine):

- **`IotsaController` owns the state machine** -- authoritative `currentMode`,
  transitions, gate, timeout, effects.
- **`IotsaConfigMod` keeps the REST/web API** -- `/api/config` continues to carry
  `currentMode` / `requestedMode` / `modeTimeout`; the `/config` form keeps the
  mode-request buttons. `iotsa config requestedMode=2` and existing scripts keep
  working. `putHandler` translates a `requestedMode` write into
  `iotsaController.requestMode(...)`; the GET reads it back from `iotsaStatus`.
- **`IotsaController` publishes `currentMode` into `iotsaStatus` every tick**, like
  `wifiStationConnected`. `inConfigurationMode()` becomes a convenience reader on
  `iotsaStatus` -- so the dozen callers (auth modules, battery, `iotsaConfig` itself)
  never reach into a module-owned object.

Principle: **state belongs with the code it most constrains, not the code that displays
it.** Mode barely constrains Config (one boolean gates edits); it pervades the
controller.

## Module responsibilities after the split

- **`IotsaConfigMod`** -- identity (`hostName`, cert) + its config UI + persistence;
  keeps the `currentMode`/`requestedMode` API surface.
- **`IotsaRunmodeMod`** (new) -- the *external control surface* onto `IotsaController`:
  reboot, reboot-into-OTA, enable/disable WiFi at runtime, mode transitions. Thin glue.
  **Core-tier: always present, unconditionally `ensure()`d like `IotsaConfigMod`** (the
  #195 / #85 mechanism), never an optional add. It carries **both** a REST/web surface
  and a **BLE service** with the reboot + mode-transition control characteristics -- a
  BLE client that finds the device also wants to steer it. Because RunmodeMod is now
  never-omittable, that BLE service is present on every BLE-enabled iotsa device, which
  is what makes it a viable home for the control characteristics (see
  [#233](https://github.com/cwi-dis/iotsa/issues/233) -- filed before the core-tier
  decision, which weakens its "RunmodeMod might be skipped" objection). The bare
  "this is an iotsa device" *identification* UUID (for the Python CLI's BLE discovery
  heuristic) is a separate sub-decision -- see Open questions.
- **`IotsaBatteryMod`** -- shrinks to battery *hardware*: voltage/USB ADC sensing, the
  pins, `correctionVBat`, the `180F` BLE service. Exposes `onUSBPower()` for the sleep
  policy to read; no longer hosts the sleep state machine, the sleep config, the
  watchdog, or the `extendCurrentModeCallback`. The sleep-adjacent BLE gesture
  (`doSoftReboot` / `allowBLEConfigModeSwitch`) moves to `IotsaRunmodeMod`. See step 3
  of "Remaining work".
- **`IotsaWifiMod`** -- unchanged from the `wifi-controller-design.md` outcome: thin
  glue over `IotsaWifiDriver` + `IotsaWifiController`.

## Transition strategy

Cross-version compatibility is worth carrying **only for the REST `/api/config` mode
keys** -- `currentMode` / `requestedMode` / `modeTimeout` / `reboot` -- so a newer
Python CLI can drive an older device and vice versa. `IotsaConfigMod` keeps those as
`[[deprecated]]` forwarders onto `/api/runmode` indefinitely (until the CLI itself moves).

The **C++ `iotsaConfig.*` forwarders** (`networkIsUp()`, `requestReboot()`,
`inConfigurationMode()`, `extendCurrentMode()`, …) are pure transitional cruft --
downstream is compiled against a fixed iotsa version, so there is no cross-version
story. They exist so the ~20 downstream repos keep building across the one release
where #106 lands. **Sweep them onto the new names (`iotsaController.*` / `iotsaStatus.*`)
when #106 hits `develop`, then delete every C++ forwarder the release after.** New moves
from here on (the step-3 sleep primitives) skip the forwarder entirely: rename the
in-tree callers in the same commit, let the downstream sweep pick up the rest.

## Landed so far

- `9d8a93d` -- `config_mode` typedef renamed to `iotsa_mode` (values unchanged; REST API
  and Python tool untouched). The device mode is a distinct concept from "runmode".
- `iotsaStatus` split out of `IotsaConfig`: the `wifiEnabled` / `wifiStationConnected` /
  `wifiApActive` / `mdnsEnabled` fields and `networkIsUp()` / `getBootReason()` /
  `printHeapSpace()` now live in `IotsaStatus` (`src/iotsaStatus.{h,cpp}`), a plain
  global struct. `iotsaConfig` keeps `[[deprecated]]` forwarders for the three methods;
  the fields had no downstream users so they moved without aliases. `networkIsUp()` now
  reads `wifiStationConnected` directly (was `wifiMode == IOTSA_WIFI_NORMAL`).
  `getStatusColor()` stays in `iotsaConfig` until its precedence rework (still reads
  `wifiMode` + `configurationMode`).
- `IotsaController` skeleton (`src/iotsaController.{h,cpp}`): a framework global, ticked
  from `IotsaApplication::loop()`. So far it owns only the deferred-reboot timer, moved
  from `IotsaConfig::loop()` (now deleted). `iotsaConfig.requestReboot()` is a
  `[[deprecated]]` forwarder -- it has many downstream callers (6 in lissabon alone).
- The `iotsa_mode` state machine moved into `IotsaController`: `currentMode()` /
  `requestedMode()` / `requestMode()` / `allowRequestedConfigurationMode()` /
  `beginConfigurationMode()` / `endConfigurationMode()` / `extendCurrentMode()` /
  `inConfigurationMode()` / `modeName()` / `factoryReset()`, plus `begin()` (the boot
  anti-tamper gate, out of `IotsaConfigMod::setup()`) and auto-expiry in `tick()` (out
  of `IotsaConfigMod::loop()`). Persistence is now a one-shot mailbox file
  `/config/pendingmode.cfg` written only by `requestMode()` and consumed+deleted by
  `begin()` -- no mode transition calls `configSave()` any more, and `config.cfg` lost
  its `mode` key. Bench-verified on lolin32: config-mode entry via `configMode` + RESET,
  rejection via `configMode` + software reboot (request consumed either way).
  `IotsaConfigMod` keeps the whole REST/web surface, retargeted at `iotsaController.*`;
  `iotsaConfig.*` has `[[deprecated]]` forwarders. `configurationModeTimeout` (the
  `rebootTimeout` setting) stays in `config.cfg` / `iotsaConfig` for now.
- `inConfigurationOrFactoryMode()` dissolved: `IotsaWifiMod` now publishes
  `iotsaStatus.wifiConfigured` (an SSID is set), and the 9 call sites (6 in
  `iotsaWifi.cpp`, 3 in `iotsaConfigMod.cpp`) use the shared `iotsaConfigSettingsWritable()`
  helper in `iotsaController.h`: `inConfigurationMode() || (!wifiConfigured && wifiApActive)`.
  The `IOTSA_WIFI_FACTORY` enum value is now read only by `getStatusColor()` (its
  removal is part of the `iotsa_wifi_mode` deletion below). No downstream callers, so
  the method was deleted outright, no forwarder.
- Runtime WiFi radio enable/disable rewired (fixes the latent regression). `IotsaController`
  gained a thin `setWifiRadioEnabled()` / `wifiRadioEnabled()` flag; `IotsaWifiMod`
  combines it with `wifiDisabledOnBoot` in `_wifiRadioWanted()` and feeds
  `_controller.setRadioEnabled()` every `loop()` (not just once at setup). `iotsaBattery`
  (BLE-enable, sleep-wifi-off, `disableSleepOnWiFi`), the `wifiDisabled` REST PUT and its
  reply are off the `wifiMode` enum now. Dead `wantWifiModeSwitchAtMillis` removed
  (BLE's `wantBleModeSwitchAtMillis` twin stays -- still read by `iotsaBLEServer`).
- `iotsa_wifi_mode` enum + `wifiMode` field deleted. Last readers rewired:
  `getStatusColor()` is a faithful translation of the old `wifiMode` switch onto
  `iotsaController.currentMode()` + the `iotsaStatus.wifi*` booleans (same colours, same
  precedence -- the real LED-semantics rework, flash patterns etc., stays
  [#176](https://github.com/cwi-dis/iotsa/issues/176)); the `privateWifi` reply is
  `wifiApActive && !wifiStationConnected`; `IotsaWifiMod::info()` shows
  `staState()`/`apState()` instead. No downstream users of the enum.
- `IotsaRunmodeMod` (`src/iotsaRunmode.{h,cpp}`) -- step 1 of "Remaining work"
  below. Core-tier, `ensure()`d unconditionally by `IotsaApplication::setup()` right
  after `IotsaConfigMod`. `/api/runmode` (GET+PUT) + the `/runmode` page: mode
  requests, reboot, runtime WiFi/BLE radio toggles, factory-reset -- all thin glue
  over `iotsaController.*`. A BLE control service (UUID `6E5D0001-...`) with
  `currentMode` (READ) / `requestedMode` (WRITE) / `reboot` (WRITE), writes acted on
  from `loop()` -- folds in the reboot-over-BLE half of
  [#233](https://github.com/cwi-dis/iotsa/issues/233). `IotsaConfigMod` lost the mode
  radios / factory-reset from its `/config` page and the mode blurb from its `info()`;
  `/api/config` keeps `currentMode` / `requestedMode` / their timeouts / `wifiDisabled`
  / `reboot` as `[[deprecated]]` forwarders for one release (Python CLI still targets
  `/api/config`). Reuses the `config` auth right, not a new `runmode` one. The
  unauthenticated REST reboot/mode PUT was carried over verbatim -- to be fixed with
  the permission-model work.
- Radio-enablement policy moved into `IotsaController` -- step 2 of "Remaining work"
  below (`8d717ca` WiFi, `820a98e` BLE). `wifiRadioWanted()` / `bleRadioWanted()`
  compose a boot seed (`begin()` reads `!wifiDisabledOnBoot` / `!bleDisabledOnBoot`),
  a runtime flag moved by `setWifiRadioEnabled()` / `setBleRadioEnabled()` (REST/web
  toggles, battery sleep, the BLE "enable WiFi" command), and mode forcing on top
  (CONFIG+OTA force WiFi on; CONFIG forces BLE on, OTA does not). `IotsaWifiMod`
  feeds `wifiRadioWanted()` to `IotsaWifiController` every tick; `IotsaBLEServerMod`
  change-detects `bleRadioWanted()` in `loop()` (replacing the
  `wantBleModeSwitchAtMillis` armed timer). Deleted along the way:
  `IotsaWifiMod::_wifiRadioWanted()`, `iotsaConfig.bleMode`, `wantBleModeSwitchAtMillis`,
  the `iotsa_ble_mode` enum. Fixed a latent regression: on a `wifiDisabledOnBoot`
  device the old `&&` short-circuited away any runtime `setWifiRadioEnabled(true)`.
  Builds green across the WiFi+BLE / WiFi-only / BLE-only / neither flag matrix; not
  bench-tested.

## Remaining work (ordered)

The `IotsaConfig` identity/status/controller split (above) is done and bench-proven.
What is left is moving the two policy tenants (radio, sleep) into `IotsaController` and
building the module layer on top. Ordering matters: the control-surface module is pulled
*ahead* of the two policy moves so they land their external knobs in their final home
instead of parking them in `IotsaConfigMod` / `IotsaBatteryMod` and moving them later.

1. **`IotsaRunmodeMod` -- control surface (REST + BLE).** *Done* (see "Landed so
   far" above): `0bef36b` (REST + web), `5690097` (BLE control service). Thin glue over
   `iotsaController.*`, core-tier `ensure()`d, done first so steps 2-3 have a real home
   for their toggles.
2. **Radio-enablement policy into `IotsaController`.** *Done* (see "Landed so far"):
   `8d717ca` (WiFi), `820a98e` (BLE). The `wifiDisabled` PUT stays parked in
   `IotsaConfigMod` as a forwarder; canonical form is in `IotsaRunmodeMod`
   (`/api/runmode`) already.
3. **Sleep/wake out of `IotsaBatteryMod` (folds in the old step 4).** Two parts:
   - **`IotsaSleepPolicy`** -- a dedicated policy object (`src/iotsaSleepPolicy.{h,cpp}`,
     class held by value as `IotsaController::_sleep`), pure logic, no platform headers,
     always compiled. Owns the inhibit primitives moved from `IotsaConfig`
     (`pauseSleep()` / `resumeSleep()` / `postponeSleep(ms)` / `canSleep()`,
     `pauseSleepCount`, `postponeSleepMillis`, `activityExtraWakeDuration`), the sleep
     config moved from `IotsaBatteryMod` (`sleepMode`, `sleepDuration`, `wakeDuration`,
     `bootExtraWakeDuration`, `disableSleepOnWiFi`, `disableWiFiOnSleep`,
     `disableSleepOnUSBPower`), the wake-window state (`millisAtWakeup`,
     `didWakeFromSleep`), and the decision: `decide(bool onUsbPower)` ->
     `{IotsaSleepMode mode, uint32_t durationMs}` or `NONE`, folding in the
     mode / WiFi-coupling / inhibit / wake-window-timing checks that were scattered
     through `IotsaBatteryMod::loop()`. `canSleep()` also returns false in CONFIG/OTA
     mode (was a separate check). No C++ forwarders on `iotsaConfig` -- the in-tree
     callers (`iotsaApiRest`, `iotsaHttpServer`, `iotsaInput`, `iotsaBLEClient`,
     `iotsaBLEServer`) are renamed to `iotsaController.*` in the same commit; downstream
     is swept when #106 hits `develop`.
   - **`extendCurrentModeCallback` deleted.** `IotsaController::extendCurrentMode()`
     calls `_sleep.postponeSleep(0)` (activity => bump the wake window by
     `activityExtraWakeDuration`) instead of `_extendCb()`. That single mechanism
     replaces `IotsaBatteryMod`'s `millisAtWakeup = millis()` bump.
     `setExtensionCallback()` / `_extendCb` gone (only battery used it; the watchdog
     re-arm it also did is redundant -- `loop()` feeds the watchdog every iteration).
   - **Platform execution folds into `IotsaRunmodeMod`** (no new module -- "runmode" *is*
     the sleep/wake rhythm, and RunmodeMod is always `ensure()`d so downstream needs no
     new instantiation). Guarded by `#ifdef IOTSA_HAS_SLEEP`: the `_notifySleepWakeup()`
     module walk, the watchdog timer + ISR, the CPU-frequency knobs, `pinDisableSleep`,
     and the `esp_light_sleep_start` / `esp_deep_sleep_start` / RTC-domain / radio-off /
     BLE pause-resume machinery. RunmodeMod's `loop()` calls
     `iotsaController.sleep().decide(batteryMod.onUSBPower())` and executes the result;
     it gains the sleep knobs in `/api/runmode` + the `/runmode` page. `IOTSA_HAS_SLEEP`
     is a stage-5 derived flag in `iotsaBuildOptions.h`:
     `defined(IOTSA_WITH_SLEEP) || (defined(IOTSA_WITH_BLE) && !defined(IOTSA_WITHOUT_SLEEP))`
     -- BLE is today's proxy for "off-grid / battery device"; retune later if an
     `IOTSA_WITH_BATTERY` flag ever lands. `IOTSA_WITH_SLEEP` / `IOTSA_WITHOUT_SLEEP`
     documented in stage 3, neither `#define`d there.
   - **`IotsaBatteryMod` shrinks to ADC**: `pinVBat` / `pinVUSB` / ranges /
     `correctionVBat` / `_readVoltages()` / levels + `/battery` + `/api/battery` + the
     `180F` BLE service. Exposes `bool onUSBPower()` for RunmodeMod to read. The
     `doSoftReboot` / `allowBLEConfigModeSwitch` gesture cluster (`E4D90003` "Reboot
     with WiFi", the prove-physical-presence path) folds into RunmodeMod's existing BLE
     control service -- `doSoftReboot==2` "promote pending mode" becomes one added
     characteristic there. `IotsaBatteryMod::setPinDisableSleep()` /
     `allowBLEConfigModeSwitch()` are renamed to `IotsaRunmodeMod` methods; the
     downstream callers (lissabon ×4/×4, iotsaRGBWSensor) are swept with #106.
4. **Persisted-settings tidy + land on develop.** Move `configurationModeTimeout`
   (`rebootTimeout`) off `iotsaConfig` per its final owner; decide where the sleep
   config persists (`/config/sleep.cfg` migrated from `battery.cfg`, loaded by
   RunmodeMod, pushed into `_sleep`); CHANGELOG line; merge.

Separable follow-ons (own issues, not gating the merge):

- BLE server driver/controller split -- the near-trivial 2-state case that should fall
  out of the WiFi driver/controller pattern.
- Mode-declaration / reject-unregistered-mode -- the
  [#174](https://github.com/cwi-dis/iotsa/issues/174) gap: modules declare which
  `iotsa_mode` they implement; reject a request nobody registered for.
- Sleep-inhibit publish/subscribe -- subsystems register inhibit signals,
  `IotsaController` aggregates; ties to
  [#105](https://github.com/cwi-dis/iotsa/issues/105).
- Identification-UUID home for BLE discovery -- the rest of [#233].

## Deferred (was "slice 4"; folds into this work)

- `getStatusColor()` LED-semantics rework (flash for hunting, etc.) -- [#176].
- Collapse `iotsaConfigSettingsWritable()` -> `inConfigurationMode()` once "no SSID =>
  config mode" lands.
- "No SSID configured => enter config mode" so `inConfigurationMode()` alone gates
  wifi-cred writes and there is no `IOTSA_WIFI_FACTORY` pseudo-mode. `_wantApUp()` in
  `IotsaWifiController` already anticipates this.

## Open questions

- Name for the status-bus global: `iotsaStatus` vs `iotsaState` vs other.
- Whether `IOTSA_MODE_CONFIG` / `IOTSA_MODE_OTA` should eventually become a
  granted-permission set with a shared expiry rather than one-of-N enum values (so a
  device can hold both at once). Deferred -- low harm in keeping them separate, easy to
  change later.
- Where `config.cfg` persistence lands once `hostName` is nearly its only remaining
  key: one file written cooperatively, or per-owner files.
- Home for the bare "this is an iotsa device" BLE *identification* service UUID (what
  the Python CLI's discovery heuristic should match instead of the standard
  Battery-Service coincidence). [#233](https://github.com/cwi-dis/iotsa/issues/233) as
  filed says `IotsaBLEServerMod`, on the grounds that any optional module (RunmodeMod
  included) could be skipped. The core-tier decision for `IotsaRunmodeMod` undercuts
  that for BLE-enabled devices -- so the choice is now "identity is a BLE-server
  concern, keep it there" vs. "fewer services, put it on RunmodeMod's control service
  which is present anyway". The reboot/mode *control* characteristics are settled
  (RunmodeMod, step 1); only the identification UUID is open. Blocks nothing in
  steps 1-5.
