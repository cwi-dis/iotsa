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
  Later gains the BLE "this is an iotsa device" service (discovery + the reboot/mode
  control characteristics on one service -- a BLE client that finds the device also
  wants to steer it). Tracked in a separate issue.
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

## Deferred (was "slice 4"; folds into this work)

- Delete the `iotsa_wifi_mode` enum + `wifiMode` field; its status values are already
  superseded by `IotsaWifiController` + the `iotsaStatus` fields.
- Rewire runtime radio enable/disable (`wifiDisabled` REST toggle, battery
  sleep-wifi-off, BLE enable-wifi) -- currently a **latent regression**: earlier #106
  slices stopped `IotsaWifiMod` polling `wantWifiModeSwitchAtMillis`, so these paths no
  longer reach the controller. `wantWifiModeSwitchAtMillis` itself has zero readers and
  can go.
- `getStatusColor()` rework -- becomes a free function over `iotsaStatus` with an
  explicit precedence: factory-reset > maintenance window (CONFIG/OTA colour) >
  connectivity problem (hunting = flash, fallback-AP / unprovisioned = solid, possibly
  tinted onto the mode colour) > normal. Status-LED colour choices tracked in
  [#176](https://github.com/cwi-dis/iotsa/issues/176).
- Collapse `inConfigurationOrFactoryMode()` -> `inConfigurationMode()` (9 call sites: 6
  in `iotsaWifi.cpp`, 3 in `iotsaConfigMod.cpp`).
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
