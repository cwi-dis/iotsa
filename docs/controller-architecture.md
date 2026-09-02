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
- **`IotsaBatteryMod`** -- shrinks to battery *hardware*: voltage sensing, the pins,
  its own config. Feeds settings into `IotsaController`; no longer hosts the sleep
  state machine or the `extendCurrentModeCallback`.
- **`IotsaWifiMod`** -- unchanged from the `wifi-controller-design.md` outcome: thin
  glue over `IotsaWifiDriver` + `IotsaWifiController`.

## Transition strategy

`iotsaConfig` stays a **compatibility facade for one release**: members that move to
`iotsaStatus` / `iotsaController` / `iotsaRunmode` remain callable as
`iotsaConfig.networkIsUp()` / `iotsaConfig.postponeSleep()` etc. via thin `[[deprecated]]`
forwarders, so nothing downstream breaks the day the split lands. The release after,
sweep the ~20 downstream repos to the new names and drop the forwarders.

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

## Remaining work (ordered)

The `IotsaConfig` identity/status/controller split (above) is done and bench-proven.
What is left is moving the two policy tenants (radio, sleep) into `IotsaController` and
building the module layer on top. Ordering matters: the control-surface module is pulled
*ahead* of the two policy moves so they land their external knobs in their final home
instead of parking them in `IotsaConfigMod` / `IotsaBatteryMod` and moving them later.

1. **`IotsaRunmodeMod` -- control surface (REST + BLE).** Thin glue over what
   `IotsaController` already exposes (`requestReboot()`, `requestMode()`,
   `setWifiRadioEnabled()`), plus the core-tier unconditional-`ensure()` wiring. The BLE
   service and its reboot / mode-transition characteristics land here now (folds in the
   control-characteristic half of [#233](https://github.com/cwi-dis/iotsa/issues/233)).
   **Depends on nothing in steps 2-3** -- the reboot path is fully wired already. Done
   first precisely so steps 2 and 3 have a real home for their toggles.
2. **Radio-enablement policy into `IotsaController`.** The `_radioPolicy` sub-policy:
   boot flags (`wifiDisabledOnBoot`, `bleDisabledOnBoot`), runtime toggles, "current
   mode forces radios up", BLE-server enable. The `wifiDisabled` PUT (parked in
   `IotsaConfigMod` by the landed radio-enable rewire) moves to `IotsaRunmodeMod`.
3. **Sleep/wake policy into `IotsaController`.** The `_sleepPolicy` sub-policy: the
   sleep decision, `pauseSleep()` / `postponeSleep()` / `canSleep()`, the WiFi<->sleep
   coupling (`disableSleepOnWiFi`, `disableWiFiOnSleep`), wake-window timing,
   `activityExtraWakeDuration`. Deletes the `extendCurrentModeCallback` hop into
   `IotsaBatteryMod`. Sleep knobs move to `IotsaRunmodeMod`'s UI.
4. **`IotsaBatteryMod` shrink.** Falls out of steps 2-3: battery module drops to
   voltage / USB ADC sensing + its own config, feeding settings into `IotsaController`.
   Depends on 2 and 3.
5. **Persisted-settings tidy + land on develop.** Move `configurationModeTimeout`
   (`rebootTimeout`) off `iotsaConfig` per its final owner; CHANGELOG line; merge.

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
