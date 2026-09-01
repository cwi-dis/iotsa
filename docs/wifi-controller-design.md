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
  on ESP8266), reduced to a small enum: `NO_AP_FOUND`, `AUTH_FAIL`, `OTHER`.
  `WiFi.status()` is too coarse and too flaky to drive policy from.
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
| `STA_HUNTING` | a connect attempt failed; retrying on a cadence set by (proven x reason), see below |

`sta_lost` from `STA_CONNECTED` goes to `STA_HUNTING`. The SDK's own
`setAutoReconnect(true)` handles the fast transient blip (~1 s) below this state
machine; anything longer surfaces as `sta_failed`/`sta_lost` and is handled uniformly by
`STA_HUNTING`. There is no separate "was previously connected" code path.

### AP state is a *derived fact*, not a state

The AP is up if **any** of:

- configuration mode is active (see "Config-mode interactions")
- STA has been in `STA_HUNTING` past its escalation delay (see below)
- an explicit request (e.g. a physical config button, future work)
- the AP-client hold is active (see below)

Otherwise the AP is down. No `IOTSA_WIFI_FACTORY`, no `IOTSA_WIFI_NOTFOUND`, no
`IOTSA_WIFI_SEARCHING` as AP-implying enum values. "Device is only reachable on its own
AP" (`privateWifi` in the `/api/config` reply) becomes the derived
`ap.up && !sta.connected`.

### Proven-credentials record

One small record, persisted next to ssid/password:

```
{ ssid, psk, bssid, channel, lastGoodMillis }
```

- `proven` = we have completed at least one `sta_got_ip` with the *current* ssid+psk.
- Set on the first `sta_got_ip`. Also refresh `bssid`/`channel`/`lastGoodMillis` on every
  `sta_got_ip`.
- Cleared when ssid or psk *actually* changes (compare first -- a no-op re-entry of the
  same value must not clear it).

`bssid`/`channel` feed the targeted `startStation(ssid, psk, channel, bssid)` fast path
(and, on deep-sleep devices, belong in RTC memory). A targeted connect skips the scan,
which is also the only AP + STA-coming-up combination that coexists cleanly on one radio
(see "AP/STA channel constraint").

## Failure handling and retry policy

The radio cannot distinguish "correct SSID whose AP is currently off/out of range" from
"SSID that was mistyped and never existed" -- both yield `NO_AP_FOUND`. So the retry
policy branches on **proven x reason**, not on failure reason alone and not on
connection history:

| | `NO_AP_FOUND` | `AUTH_FAIL` |
| --- | --- | --- |
| **unproven** creds | probably a setup mistake (SSID or password). A few quick retries, then escalate. | same -- probably a setup mistake. |
| **proven** creds | transient: the network is real and correct, just not here now. Patient retry. | the password changed under us. A few retries, then escalate. |

"Escalate" = the escalation delay expires and the AP-up derivation turns true.

Escalation delays:

- unproven (any reason): ~30-60 s (after a few quick retries)
- proven + `AUTH_FAIL`: ~short
- proven + `NO_AP_FOUND`: **~5 minutes**, then escalate anyway. Not "wait forever" --
  eventually the AP must come up so a human can request config mode -> reboot ->
  reconfigure.
- `OTHER`: treat as the patient case, or a middle path -- open question.

Retry cadence within `STA_HUNTING`: former `10 x IOTSA_WIFI_TIMEOUT` (300 s) constant
was arbitrary and is gone. Cadence is per-reason -- short/backoff for `AUTH_FAIL` (do
not hammer a failed auth; some APs rate-limit or temp-ban), longer / sleep-driven for
`NO_AP_FOUND`.

Note `AUTH_FAIL` here specifically means "hammering it is counterproductive"; the retry
loop must actually back off, not retry every few seconds.

### AP-client hold

The moment a client associates to our AP (`ap_client_connected`), arm a "no
channel-hopping scan for ~2 minutes" hold. Re-arm on each new client connect (optionally
also on each HTTP request). The disruptive channel-hopping hunt resumes only when the
hold has expired **and** `ap.clientCount == 0`.

Rationale: someone configuring will disconnect/reconnect (WiFi flakiness, switching
devices, the config flow itself), and the radio channel-hopping mid-session would knock
them off. A timed hold gives a stable configuration window robust to transient drops.

"Suspend hunting" means suspend the **channel-hopping scan** only. Home-channel-only
retry and targeted-BSSID reconnect (proven network) do not disrupt the AP and may
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

This is the strongest argument for the proven-record carrying `{bssid, channel}`: a
proven network gives the targeted reconnect, the only AP + STA-coming-up combination
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

- `OTHER` failure reason -- patient handling, or a middle path?
- Proven + `NO_AP_FOUND` for a very long time (hours) on a *relocated* device that will
  never see its network again -- currently "AP up after 5 min, stays up." Any need for a
  longer-term behaviour (deep-sleep-and-retry for battery devices is a runmode decision,
  not this controller's -- the controller just publishes "hunting, AP up, 0 clients").
- Exact per-reason retry cadences and escalation-delay values -- to be tuned against
  real hardware.

## Acceptance tests

To be written as real tests and held to; "the redesign deletes that bug class" is a
claim to verify, not assume.

1. **Fresh device, no explicit reboot.** Boot with no ssid -> config mode, AP up, no
   timeout. Submit ssid+psk over the AP -> device connects to the new network without a
   reboot. (Today: fails -- stuck in factory/AP mode until a power cycle.)
2. **Config-mode exit does not churn STA.** Enter config mode while STA connected, then
   let it time out / leave explicitly -> AP drops, STA connection is uninterrupted.
   (Today: full STA disconnect/reconnect cycle.)
3. **Proven network briefly absent.** Established connection, network goes away for
   < 5 min, comes back -> device reconnects, AP never came up, no config-mode change.
4. **Mistyped ssid.** Unproven creds, `NO_AP_FOUND` -> AP up within ~1 min, loud status.
5. **Password changed.** Proven creds, `AUTH_FAIL` -> a few retries, backoff (not a
   tight loop), AP up, loud status.
6. **AP-client hold.** Client associates to the AP -> no channel-hopping scan for the
   hold window even if `clientCount` briefly hits 0.
7. **AP/STA channel behaviour** -- bench, per chip: AP up + hunting, observe whether the
   scan hops and whether AP clients survive; confirm the mutually-exclusive model holds.
8. **Security invariant.** Device in post-escalation AP-fallback -> attempt to change a
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
