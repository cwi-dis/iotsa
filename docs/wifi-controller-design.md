# WiFi controller redesign

**Status:** design, not yet implemented. Branch `106-runmode-module`.

This is step 1 of the real work behind [cwi-dis/iotsa#106](https://github.com/cwi-dis/iotsa/issues/106).
The `IotsaBatteryMod` -> `IotsaRunmodeMod` rename is window-dressing; the substance is
deciding where each tangled responsibility in `IotsaConfig` / `IotsaConfigMod` /
`IotsaBatteryMod` / `IotsaWifiMod` belongs. The WiFi mode machinery is the messiest
tenant of that decision, so it gets designed first: the controller's shape then informs
the `IotsaRunmodeMod` / `IotsaConfigMod` split rather than the split being guessed at.

See the 2026-09-01 findings comment on #106 for the full catalogue of what is wrong with
the current `iotsaWifi.cpp`. The short version: one `iotsa_wifi_mode` enum fuses two
orthogonal concerns (STA connection state and "is the AP also up"), three hand-rolled
timers do one job, a config-file save triggers a state transition as a side effect,
there is no single writer of the state, and there is at least one confirmed latent bug
(a fresh device that is given credentials without an explicit `reboot` never leaves
factory/AP mode -- `wifiMode` has no FACTORY->NORMAL transition except at boot).

## Root diagnosis

`IotsaWifiMod` fuses **policy** (decide when to retry, when to fall back to AP, when to
switch mode) with **mechanism** (drive the radio). That fusion is why nothing outside it
can safely query or command it, and why it has to make every decision alone inside its
own `loop()`.

The redesign separates the two: `IotsaWifiMod` becomes a thin driver, and a single
controller owns all policy and all state.

## Driver: `IotsaWifiMod` shrinks to mechanism

The driver knows how to talk to the ESP8266/ESP32 WiFi API and nothing else. No timers,
no retry logic, no reading `configurationMode`, no `wifiMode` field it owns and mutates.

Imperative operations (fire, report outcome):

- `startStation(ssid, password, channel = 0, bssid = nullptr)` -- with `channel`/`bssid`
  given, a targeted connect (no scan); without, an implicit scan-then-connect.
- `stopStation()`
- `startAP(name)`
- `stopAP()`
- `reinitStack()` -- `WiFi.disconnect(true)` -> `WiFi.mode(WIFI_OFF)` -> `WiFi.mode(WIFI_STA)`,
  for recovering a wedged stack. Replaces today's `ESP.restart()` sledgehammer.

Events surfaced to the controller (via the platform WiFi event callbacks, not polled
`WiFi.status()`):

- `sta_connected` / `sta_got_ip` (carries channel + BSSID)
- `sta_failed(reason)` -- `reason` taken from the disconnect event
  (`info.wifi_sta_disconnected.reason` on ESP32, `WiFiEventStationModeDisconnected.reason`
  on ESP8266), reduced by the driver to a small enum: `NO_AP_FOUND`, `AUTH_FAIL`, `OTHER`.
  The reduction folds `HANDSHAKE_TIMEOUT` / `4WAY_HANDSHAKE_TIMEOUT` into `AUTH_FAIL` --
  on many APs a wrong password surfaces as a handshake timeout rather than a clean auth
  reject. `WiFi.status()` is too coarse and too flaky to drive policy from.
- `sta_lost` -- was connected, connection dropped
- `ap_client_connected` / `ap_client_disconnected` (carries count)

Introspection -- `readActualState()` returns:

- `mode` -- `OFF` / `STA` / `AP` / `STA_AP`
- `sta.configuredSsid`, `sta.configuredPsk` -- the *configured* target, readable even
  mid-connect. ESP8266: `WiFi.SSID()` / `WiFi.psk()` already read the SDK station-config
  struct. ESP32: `WiFi.SSID()` is connected-only, so use
  `esp_wifi_get_config(WIFI_IF_STA, &conf)` (`conf.sta.ssid` / `conf.sta.password`).
- `sta.linkStatus`
- `ap.ssid`, `ap.clientCount`

## Controller: owns all policy and state

The controller is the sole writer of connection state. Everyone else queries it (or
subscribes to change notifications). The physical home of the controller object -- an
expanded `iotsaConfig`, a new `iotsaControl`, or `IotsaRunmodeMod` -- is deliberately
**not** decided here; that is the #106 split, and it is easier once this shape is
concrete. The controller is written as its own state-machine type with its own header so
the eventual move is mechanical.

### STA sub-state

A real state machine, not an enum value shared with AP concerns:

| state | meaning |
| --- | --- |
| `STA_OFF` | not attempting -- no credentials, or explicitly disabled (`wifiDisabledOnBoot`) |
| `STA_CONNECTING` | connect issued, deadline armed |
| `STA_CONNECTED` | have an IP |
| `STA_HUNTING` | a connect attempt failed; retrying on a cadence set by failure reason, see below |

`sta_lost` from `STA_CONNECTED` goes to `STA_HUNTING`. The SDK's own
`setAutoReconnect(true)` handles the fast transient blip (~1 s) below this state
machine; anything longer surfaces as `sta_failed`/`sta_lost` and is handled uniformly by
`STA_HUNTING`. There is no separate "was previously connected" code path.

### AP state is a *derived fact*, not a state

The AP is up if **any** of:

- configuration mode is active (see "Config-mode interactions")
- STA has been failing for longer than the escalation delay (see below)
- an explicit request (e.g. a physical config button, future work)
- the AP-client hold is active (see below)

Otherwise the AP is down. No `IOTSA_WIFI_FACTORY`, no `IOTSA_WIFI_NOTFOUND`, no
`IOTSA_WIFI_SEARCHING` as AP-implying enum values. "Device is only reachable on its own
AP" (`privateWifi` in the `/api/config` reply) becomes the derived
`ap.up && !sta.connected`.

### Fast-reconnect cache

One small record, **RTC RAM only** (survives deep sleep, lost on a true power-loss
reboot -- that's fine, it is regenerated cheaply on the first connect after a cold boot):

```
{ ssid, bssid, channel, lastGoodMillis }
```

- Refreshed on every `sta_got_ip`.
- Discarded when the configured ssid changes (a cached BSSID for a different network is
  useless).

`bssid`/`channel` feed the targeted `startStation(ssid, psk, channel, bssid)` fast path
(~200 ms vs a ~2-4 s cold scan+connect). A targeted connect also skips the scan, which
is the only AP + STA-coming-up combination that coexists cleanly on one radio (see
"AP/STA channel constraint"). This is a pure speed optimisation -- it carries no policy
role.

## Failure handling and retry policy

**The AP-raise decision is unconditional.** Sustained STA failure -> a few quick retries
-> after the escalation delay (~1 minute) the AP-up derivation turns true. It does not
matter whether the network was ever reachable before, or why the connect failed. The
radio cannot tell "correct SSID whose AP is off" from "SSID that was mistyped" anyway
(both give `NO_AP_FOUND`), and it does not need to: an early AP is what you want in both
the setup-mistake case *and* the just-carried-this-device-somewhere case.

Whether the AP then *persists* is not this controller's concern. On a battery / off-grid
device the sleep cycle bounds it: the AP comes up ~1 minute after STA fails, then the
wake window ends, the runmode module sleeps, `disableWiFiOnSleep` turns the radio (AP
and STA both) off. So the AP is up ~1 minute per sleep cycle -- negligible, and nobody
is standing at an unattended site anyway. A mains / USB-powered / non-sleeping device
holds the AP, which is exactly right when someone has just moved it. The WiFi controller
just publishes "hunting / AP up / N clients"; runmode acts on it. There is **no**
site-policy flag and **no** proven/unproven distinction for this decision -- both were
considered and both collapse into "runmode owns AP persistence via the sleep timer."

The failure `reason` still matters, but only for **retry cadence**, not for the AP:

- `AUTH_FAIL` (incl. the folded-in handshake timeouts): back off hard. Hammering a
  failed auth is counterproductive -- some APs rate-limit or temp-ban. Retry on a
  stretching interval, not every few seconds.
- `NO_AP_FOUND` / `OTHER`: steady cadence. After a long spell with no AP found, the
  retry may stretch to once every ~10-15 min (the network genuinely is not coming back
  soon) -- a tuning detail, not a decision.

The former `10 x IOTSA_WIFI_TIMEOUT` (300 s) constant was arbitrary and is gone.

### AP-client hold

The moment a client associates to our AP (`ap_client_connected`), arm a "no
channel-hopping scan for ~1 minute" hold (value tunable). Re-arm on each new client
connect (optionally also on each HTTP request). The disruptive channel-hopping hunt
resumes only when the hold has expired **and** `ap.clientCount == 0`.

Rationale: someone configuring will disconnect/reconnect (WiFi flakiness, switching
devices, the config flow itself), and the radio channel-hopping mid-session would knock
them off. A timed hold gives a stable configuration window robust to transient drops.

"Suspend hunting" means suspend the **channel-hopping scan** only. Home-channel-only
retry and targeted reconnect via the cached BSSID+channel do not disrupt the AP and may
continue during the hold.

Caveat: L2 association is not proof a human is configuring (phones probe open APs,
remembered SSIDs auto-join and idle). `clientCount > 0` is a decent proxy; tightening it
to "client present AND an HTTP request in the last N seconds" is a possible later
refinement, not v1.

## AP/STA channel constraint

One radio. It cannot hold "softAP pinned to channel N" and "STA sweeping all channels"
at the same time. Either the scan cannot really hop (STA only ever sees co-channel APs,
never finds a target elsewhere), or it does hop and the softAP drops out on every sweep
and kicks its clients.

What coexists on one radio:

- **AP + connected STA** -- fine. Both locked to the connected AP's channel, no scanning.
- **AP + STA connecting via cached BSSID+channel** (targeted, no scan) -- fine. If the
  target channel differs from the AP channel, the STA connect yanks the softAP to the
  new channel once, then it is stable.
- **AP + STA doing a channel-hopping scan** -- the broken combination.

Design consequence: **AP-up and an active channel-hopping hunt are mutually exclusive
while the AP has a client** (see the AP-client hold). With no client, the controller may
run the hunt as a heartbeat -- "drop AP for ~5 s, full scan, re-raise AP" -- or restrict
to home-channel-only retry. It never free-runs a scan under a live AP.

This is the strongest argument for the fast-reconnect cache carrying `{bssid, channel}`:
a cached network gives the targeted reconnect, the only AP + STA-coming-up combination
that is clean.

**Must be bench-tested on real ESP8266 *and* ESP32 (plus C3 / S3) after implementation.**
The scan-during-softAP interaction is chip- and core-version-specific and barely
documented. The design must not *assume* concurrent hunt+AP works; it picks the
mutually-exclusive model and we verify what each chip actually does.

## Persistence strategy

Lean on the SDK's own credential store (flash on ESP8266, NVS on ESP32) rather than
ignoring it as today (iotsa currently keeps its own `/config/wifi.cfg`, reads it late in
`setup()`, and does a cold full scan + connect every boot -- 2-4 s every time).

- Set `WiFi.persistent(true)` **explicitly** -- do not rely on the core default, which
  has differed across core versions.
- The controller "declares" desired state idempotently: `readActualState()`, and if the
  radio is already in / coming up in exactly the wanted mode + ssid + psk, **do
  nothing**. Only a real mismatch triggers a `startStation` / `startAP` / `stop*`.
- This is not only a speed optimisation: `WiFi.begin()` with `persistent(true)` writes
  the store on every call, so "act only on mismatch" is also what keeps flash/NVS wear
  down.
- Deep-sleep devices additionally stash `{bssid, channel}` in RTC memory for a targeted
  reconnect on wake (~200 ms instead of ~2-4 s).

The introspection needed for "declare, verify, act only on mismatch" exists on both
platforms -- see the driver `readActualState()` section.

## Config-mode interactions

Decisions from the #106 design discussion, all of which shrink the config/factory
machinery:

- **Factory mode is deleted.** `IOTSA_WIFI_FACTORY` leaves the enum;
  `inConfigurationOrFactoryMode()` is deleted, not renamed -- every call site collapses
  to `inConfigurationMode()`. "Unconfigured" = `configuration mode active && ssid empty`,
  a derived condition, not a state.
- **Factory mode implies configuration mode.** A device that boots with no ssid enters
  configuration mode automatically.
- **Configuration mode does not time out while ssid is empty.** The 300 s timeout clock
  only starts once an ssid is set. (Replaces today's implicit "factory is forever" with
  an explicit rule.) "Usable credentials" for this gate = ssid non-empty (nothing
  stronger).
- **Configuration mode => the AP is always up**, for the whole duration, unconditionally.
  Replaces the scattered `if (inConfigurationMode())` AP checks in `_wifiGotoMode()`.
- **Leaving configuration mode drops the AP and touches STA not at all.** Today
  `endConfigurationMode()` cycles STA through a full disconnect/reconnect purely because
  "drop the AP" and "connect STA from scratch" share `_wifiGotoMode()`. In the new model
  it just flips the AP-up derivation to false. If STA was connected it stays connected.
- `configSave()` no longer triggers a mode transition. Persisting a setting persists a
  setting; the request handler that implies a transition triggers it explicitly.
- `getStatusColor()` needs its own rethink -- it currently switches on `wifiMode` *and*
  `configurationMode` with an "extra white tint" hack. New inputs: `configurationMode`
  (CONFIG / OTA / FACTORY_RESET), `sta.connected`, `ap.up`, radio-disabled. Whether
  `STA_HUNTING` ("SEARCHING") deserves its own colour is a call for the status-LED work
  ([#176](https://github.com/cwi-dis/iotsa/issues/176)); record the inputs here, design
  the colours there.

## Security invariant

> The settings-unlocked state (configuration mode) is reachable only by: (a) an
> authenticated request over a working connection followed by a real reboot; (b)
> physical interaction at the device; or (c) automatically at first boot when no SSID is
> configured. **Never** merely because the device is unreachable on its configured
> network.

The AP-fallback that comes up after the escalation delay does **not** unlock settings.
Concretely: intruder turns off the house WiFi, waits for every iotsa device to fall back
to its AP, reconfigures them, restores WiFi -- this must not work, and does not, because
AP-up != configuration mode. The recovery flow for a genuinely misconfigured device is:
AP comes up -> connect to it -> authenticate -> request config mode -> reboot (real
reset honours the request) -> reconfigure.

## Scheduler primitive

Every "do X at time T" in the controller (connect deadline, retry cadence, escalation
delay, AP-client hold) uses one small scheduler the controller provides -- arm a
deadline on an event, check it in the tick -- instead of the current hand-rolled
`searchTimeoutMillis` (reused for two unrelated timeouts) plus
`wantWifiModeSwitchAtMillis` plus BLE's separate copy of the same `wantBleModeSwitchAtMillis`
pattern. The BLE server's mode-switch timing moves onto the same primitive.

## Open questions

- Exact escalation delay and per-reason retry cadences -- to be tuned against real
  hardware. Starting points: escalation ~1 min; `AUTH_FAIL` backoff stretching to
  minutes; `NO_AP_FOUND`/`OTHER` steady, stretching to ~10-15 min after a long spell.
- Tightening the AP-client hold from "client associated" to "client associated AND an
  HTTP request in the last N seconds" -- a possible later refinement, not v1.
- What "explicit request" for AP-up concretely is (physical config button, etc.) --
  deferred, its own work.

Resolved during design (recorded here so they are not re-litigated): the `proven` /
`unproven` credentials distinction and a `fixed` vs `portable`/`off-grid` site-policy
flag were both considered for gating the AP-raise decision and both dropped -- the
AP-raise is unconditional, and AP *persistence* is already handled orthogonally by the
runmode sleep timer + `disableWiFiOnSleep`. `OTHER` failure reason behaves like
`NO_AP_FOUND`; `HANDSHAKE_TIMEOUT` is folded into `AUTH_FAIL` by the driver.

## Acceptance tests

To be written as real tests and held to; "the redesign deletes that bug class" is a
claim to verify, not assume.

1. **Fresh device, no explicit reboot.** Boot with no ssid -> config mode, AP up, no
   timeout. Submit ssid+psk over the AP -> device connects to the new network without a
   reboot. (Today: fails -- stuck in factory/AP mode until a power cycle.)
2. **Config-mode exit does not churn STA.** Enter config mode while STA connected, then
   let it time out / leave explicitly -> AP drops, STA connection is uninterrupted.
   (Today: full STA disconnect/reconnect cycle.)
3. **Brief outage.** Established connection, network gone for a few seconds to ~1 min,
   comes back -> SDK auto-reconnect restores it, the controller never leaves the
   connected path, AP never came up, no config-mode change.
4. **Nonexistent ssid.** Configure an ssid that is not on the air -> after a few
   retries, AP up within ~1 min, loud status.
5. **Wrong password.** `AUTH_FAIL` -> a few retries then a stretching backoff (not a
   tight loop -- verify the interval actually grows), AP up, loud status.
6. **AP-client hold.** Client associates to the AP -> no channel-hopping scan for the
   hold window even if `clientCount` briefly hits 0.
7. **Sleep bounds the AP.** Battery device, off-grid, network absent -> AP comes up ~1
   min after STA fails, then the wake window ends and `disableWiFiOnSleep` takes the
   radio down; AP is not held across the sleep.
8. **AP/STA channel behaviour** -- bench, per chip: AP up + hunting, observe whether the
   scan hops and whether AP clients survive; confirm the mutually-exclusive model holds.
9. **Security invariant.** Device in post-escalation AP-fallback -> attempt to change a
   protected setting over the AP without config mode -> rejected.

## Sequencing

1. This design.
2. Implement the driver/controller split for WiFi. The controller object lives wherever
   is convenient to wire against the existing `iotsaConfig` for now, written as its own
   type/header.
3. Bench-test thoroughly (acceptance tests above), both chips.
4. Apply the same driver/controller pattern to the BLE server (a near-trivial 2-state
   case by comparison).
5. **Only then** finalise the `IotsaRunmodeMod` / `IotsaConfigMod` split (#106), with
   the controller's real shape known.
