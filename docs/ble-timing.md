# BLE client/server timing

How `IotsaBLEClientMod`, `IotsaBLEClientConnection`, and `IotsaBLEServerMod` share the single
BLE radio, what each timing setting actually controls, and how to tune them. Written 2026-07-19
while root-causing a `control`-can't-connect-to-`stripbank` bug that turned out to be a mix of a
real units bug and genuine radio-scheduling conflicts -- the settings below are the levers that
came out of that work.

This is the technical reference: exact field names, defaults, and source-verified interplay
notes. For a plain-language explanation of the power-vs-responsiveness tradeoff and worked
examples of what happens when server and client settings are mismatched, read
[`ble-power-vs-responsiveness.md`](ble-power-vs-responsiveness.md) first.

## The single-radio constraint

An ESP32 has one BLE radio. Every role -- advertising (peripheral), scanning (observer),
initiating a connection (central), and each already-open connection -- has to share it. Two
rules, both confirmed by reading NimBLE's host source (`ble_gap.c`), govern what can run at the
same time:

- **Scanning and connecting are mutually exclusive.** `ble_gap_connect()` rejects outright
  (`BLE_HS_EBUSY`) if a scan is active, and `ble_gap_disc()` rejects (`BLE_HS_EBUSY`) if a
  connection attempt is already in progress. `IotsaBLEClientMod` enforces both directions:
  `canConnect()` blocks a new connect while scanning, and `connectingCount` blocks a new scan
  while any connect is in progress.
- **Advertising doesn't block, or get blocked by, scanning or connecting** -- confirmed by
  reading `ble_gap_adv_validate()`, which only checks whether advertising is *already* running,
  never the scan/connect state. So a device that's both a server and a client (like `control`)
  can advertise continuously while its client side scans and connects. This isn't free, though:
  it's still one radio's airtime being time-sliced, and (unverified, since this lives in the
  closed BLE controller firmware, not NimBLE's open host stack) heavy contention could still add
  jitter to any one role.
- **Open connections share one pool**, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` (default 3), across
  *both* roles combined -- an outbound client connection and an inbound server connection draw
  from the same budget. Deliberately **not** treated as something to raise by default (see
  "Known limitation" below) -- it's cheap to hit if several client connections stay open at once
  plus one server connection, and there's no interplay knob that fixes it, just headroom.

## Three deployment shapes

The settings below matter differently depending on who's on each end of the connection. Always
place yourself in one of these three before tuning:

1. **iotsa ↔ iotsa** (e.g. `control` ↔ `stripbank`). You control both ends. This is where
   *interplay* tuning pays off: a client's discovery/presence-check cadence should be matched
   against the server's advertise duration and sleep/wake cycle, because you can see and change
   both sides.
2. **non-iotsa client → iotsa server** (the `iotsa` CLI, a phone app, anything else connecting
   *to* an iotsa device's BLE server/HPS API). Only the **server-side** settings are yours to
   tune. The client's scan/retry behavior is opaque and outside your control, so server settings
   should be robust against an unknown, possibly-impatient caller rather than tuned to a specific
   known cadence.
3. **iotsa client → non-iotsa server** (an iotsa device's `IotsaBLEClientMod` connecting to a
   third-party BLE peripheral -- a commercial sensor, say). Only the **client-side** settings are
   yours. The peer's advertise interval and any sleep behavior are fixed and unknown, so client
   discovery/connect timeouts need enough margin to cope with whatever that peripheral does,
   without assuming it's another iotsa device you could coordinate with.

Each setting below is tagged with which of these three shapes it's relevant to.

## Settings reference

### Client side (`IotsaBLEClientMod`, REST `/api/bleclient`)

All of these are already configurable (added earlier in this session) and persisted to
`/config/bleclient.cfg`.

**`scan_interval`** (default 155ms) / **`scan_window`** (default 151ms)
— *Relevant to: all three shapes.*
How often the radio starts a scan window, and how much of that window is spent actually
listening (`window` ≤ `interval`; `window == interval` means continuous scanning). Lower window
relative to interval saves power (radio duty-cycles) at the cost of a higher chance of missing a
short advertisement burst. Raise `window` toward `interval` if devices are being missed; lower it
if scanning is competing too much with a server role that's also trying to advertise on the same
radio.

**`scanDurationDiscoveryMillis`** (default 11000ms)
— *Relevant to: all three shapes, but especially shape 3 (unknown peer).*
How long a *discovery* scan runs -- used when we don't yet have an address for a device (either
truly unknown, or a known device whose address we've never matched by name). Needs to be long
enough to have a reasonable chance of catching the peer's advertisement at least once. If the
peer sleeps and only advertises briefly on wake (shape 1 with a battery-powered iotsa peer, or
shape 3 with an unknown-duty-cycle peripheral), this needs to be comparable to or longer than the
peer's wake window, or discovery may never succeed inside one scan.

**`scanCooldownDiscoveryMillis`** (default 4000ms)
— *Relevant to: all three shapes.*
Minimum gap after a scan stops before the next one is allowed to start. This is the client's main
"how much airtime am I willing to spend scanning" knob. **Interplay:** a short discovery cooldown
against a server with a long sleep cycle wastes power re-scanning for a peer that can't possibly
have woken up yet -- this cooldown should be tuned against the server's `sleepDuration` (shape 1
only; unknowable for shape 3). It also interacts with connect priority: if this is too short
relative to how long a connect attempt typically takes, a discovery scan for *other* devices can
keep getting deferred (see `connectingCount` above) or -- before that fix -- could collide with
an in-flight connect and break it.

**`connectSettleTimeMillis`** (default 100ms)
— *Relevant to: all three shapes.*
Grace period after a scan stops before a connect is attempted. Exists because an immediate
connect right after `stopScanning()` has been observed to fail on real hardware -- the radio
needs a moment to actually settle out of scanning mode. Raise if connects still fail immediately
after a scan stop; there's little reason to lower it below the default.

**`connectTimeoutMillis`** (default 6000ms; lives on `IotsaBLEClientMod`, read by
`IotsaBLEClientConnection::connect()` through its owner back-pointer)
— *Relevant to: all three shapes, critically shape 3.*
How long a single `connect()` call waits for the link to establish before giving up. This is the
setting that was silently 5000x too short (6ms instead of 6s) until 2026-07-19, when it was also
made configurable and moved here from a hardcoded per-connection constant.

**`scanUnknownDurationMillis`** (default 20000ms; was the hardcoded `SCAN_UNKNOWN_DURATION_MS`
until 2026-07-19)
— *Relevant to: all three shapes.*
How long the manual "scan for unknown devices" session (REST `scanUnknown` flag, or the web
form's "Scan for N seconds" button, which now interpolates the real value) stays active before
automatically turning back off. Unlike the other durations here, this is a *session* length, not
a single scan's duration -- during the session, `updateScanning()` still runs its normal
discovery-scan/cooldown cycle (`scanDurationDiscoveryMillis`/`scanCooldownDiscoveryMillis`)
repeatedly. Conceptually related to those two (it's currently ~1.3x one full discovery cycle,
15000ms) but deliberately kept as its own independent value for now rather than computed from
them -- may be derived from the discovery cycle length in the future, but made configurable as-is
first since that's the more immediately useful change.

**`needsRescan`** (per-device bool, `IotsaBLEClientConnection`; added 2026-08-04, replacing a
prior `clearDevice()` call on every failed connect attempt -- see cwi-dis/iotsa#172)
— *Relevant to: shape 1 mainly (shapes 2/3 peers may rotate private addresses, where clearing the
address on failure could still be the right call -- not addressed here).*
Set when a connect attempt to this device fails, cleared once the device is reconfirmed reachable
(a matching advertisement, or a successful connect). Deliberately separate from `available()`
(whether we know the device's address at all, effectively permanent once learned): a failed
connect against a lightSleep peer (e.g. `stripbank`/`striplinks`, 1500ms sleep/300ms wake) usually
just means the attempt landed in the sleep window, not that the address is wrong, so it must not
force an expensive rediscovery-by-name scan. `needsDiscovery()` treats any device with
`needsRescan` set as a reason to scan, same as an unaddressed device -- triggered on the *first*
failure, not after a run of several, since a triggered scan is cheap (connects still preempt scans
the same way regardless of why the scan started).

Historical note: this replaces the old "presence-check" scan concept
(`scanDurationPresenceMillis`/`scanCooldownPresenceMillis`/`currentScanIsDiscovery`/
`allKnownDevicesSeenSince()`, removed 2026-08-04), which periodically re-verified already-addressed
devices on a timer regardless of whether anything had actually gone wrong. It turned out to have no
functional consumer -- the freshened `lastSeenAtMillis` it produced was never read by anything
except its own early-stop check -- so it just spent radio time for a cosmetic timestamp. Scanning
now only ever starts for one of three reasons: a user-requested scan, an unaddressed known device,
or a device with `needsRescan` set.

### Client side, not yet configurable

**`SCAN_START_RETRY_MS`** (hardcoded 1000ms, `iotsaBLEClient.cpp`)
— *Relevant to: all three shapes.*
Retry delay when a scan-start attempt fails -- either because NimBLE rejected it (a connect was
in progress) or because our own `connectingCount > 0` guard held it off proactively. Too short
wastes CPU/log spam re-attempting during a connect that's known to still be running; too long
delays discovery of new/missing devices unnecessarily. **Deliberately left hardcoded**: unlike
the discovery/presence durations above, this isn't a deployment-tunable -- it's a short internal
retry cadence for "try again once whatever blocked us is done," not something that varies
meaningfully by sleep/advertise configuration.

### Client side, overridable by subclasses (not REST-configurable)

**`noScheduledScanKeepOpenCapMillis`** (default 30000ms, protected member of `IotsaBLEClientMod`;
fixed 2026-07-19, was an anonymous `30000 // Random large value` local in `maxConnectionKeepOpen()`)
— *Relevant to: application design, not deployment.*
Caps how long `maxConnectionKeepOpen()` allows a connection to be held open when there's no
scheduled scan deadline to respect (`shouldUpdateScanAtMillis == 0` -- which happens not just when
idle, but also while a scan is actively running, since nothing reschedules that deadline until the
scan stops). This is a genuinely different kind of setting from everything else in this doc: it's
not a per-deployment tuning knob (no REST/persisted config), because the right value depends on
what the *application* is doing with the connection, not on server sleep cycles or client scan
cadence. Lissabon's `BLEDimmer` only needs a few seconds (`stayConnectedMillis`, capped well under
this fallback so it never actually binds today) for a quick follow-up command; a hypothetical
future module doing continuous sensor polling or an audio/data transfer over the same connection
would legitimately want this much larger. Meant to be set by a subclass's constructor, the same
way `BLEDimmer` already takes `stayConnectedMillis` as one -- not something an end user tunes
per-device.

(`connectTimeoutMillis` used to be listed here too, as a hardcoded constant -- it's above now,
in the configurable section, alongside the rest of its Millis-suffixed siblings.)

### Server side (`IotsaBLEServerMod`, REST `/api/bleserver`)

**`adv_min`** / **`adv_max`** (default -1 = NimBLE default; unit is **0.625ms**, not
milliseconds -- range 32..16384 raw units, i.e. roughly 20ms..10.24s)
— *Relevant to: all three shapes, but especially shape 2 (unknown client).*
Advertising interval bounds. Lower = more frequent advertisements = found faster by scanners, at
higher power cost. For shape 2 (an unknown external client, e.g. the `iotsa` CLI), err toward the
lower/faster end unless power is tight, since you can't coordinate an external tool's scan
cadence against your advertise interval the way you can with another iotsa device.

**`tx_power`** — not time-related, listed here only because it's the third already-configurable
field on this module. Not discussed further in this doc.

### Server side, not yet configurable

**`ADVERTISING_RETRY_MS`** (hardcoded 2000ms, `iotsaBLEServer.cpp`, added today alongside the
advertising-restart-on-failure fix)
— *Relevant to: all three shapes.*
Retry delay after a failed advertising start (e.g. `BLE_HS_ENOMEM` from the shared connection
pool being full). Too short retries uselessly fast while the pool is still exhausted; too long
means staying invisible/unconnectable longer than necessary once the pool frees up.
**Deliberately left hardcoded**, same reasoning as `SCAN_START_RETRY_MS` above -- an internal
retry cadence for "try again once whatever blocked us clears up," not a deployment-tunable.

**`minAdvertisingDurationMillis`** / **`extraDurationForConnectingMillis`** (local variables,
`resumeServer()`, `iotsaBLEServer.cpp`; fixed 2026-07-19, were hardcoded 70ms/30ms)
— *Not a good fit for independent configurability* -- correctly so now, and not just in principle.
`minAdvertisingDurationMillis` is computed from `adv_min` (`(adv_min >= 0 ? adv_min : 32) * 5 / 8
+ 50`) instead of being a fixed 70ms that only happened to be correct at the legal-minimum
interval and would silently go stale for any server configured slower (e.g. `control`'s
`adv_min=100`). The `+50` margin is calibrated to reproduce the old, already field-tested 70ms
at the legal minimum -- not a from-first-principles BLE-timing derivation, since the original
70ms comment's own arithmetic ("20ms cycle + 10ms margin = 70ms") didn't actually reconcile.
`extraDurationForConnectingMillis` stays a fixed 30ms -- connection-establishment handshake time,
independent of the advertising interval.

### lissabon side (`BLEDimmer`, `mainLedstripController.cpp` -- not in this repo, listed for
completeness since the interplay crosses the repo boundary)

**`unreachableGiveUpMillis`** (hardcoded 10000ms, member of `BLEDimmer`, `BLEDimmer.h`; was a
`BLEDimmer.cpp` file-scope constant misleadingly named `IOTSA_BLEDIMMER_CONNECT_TIMEOUT` until
2026-07-19, briefly `discoveryTimeoutMillis` before settling on this name the same day)
— *Relevant to: shape 1 (lissabon dimmers are always other iotsa devices today).*
Despite the old name, this has nothing to do with the `connect()` call itself -- it only gates
`connectionTask()`'s discovery-wait branch (`!dimmer->available()`, i.e. the device's address
isn't known yet): how long to keep a pending command alive while waiting for a discovery scan to
find the device, before giving up on it entirely (the command is dropped, not retried). It does
not stack with `connectTimeoutMillis` -- the two apply to non-overlapping phases (waiting for
discovery vs. one connect attempt once already discovered) -- so the only real constraint is that
it comfortably exceeds one realistic discovery-scan cycle. **Deliberately left hardcoded, not
made configurable**, pending
[cwi-dis/iotsa#144](https://github.com/cwi-dis/iotsa/issues/144): most of
`BLEDimmer::connectionTask()`, including this timeout, is generic connection-lifecycle
orchestration that doesn't belong in lissabon at all -- adding config plumbing for it here would
just have to be redone once it moves. Also worth reconsidering separately from configurability:
silently dropping the pending command on timeout, rather than retrying at a lower rate, is
arguably the wrong behavior regardless of what the number is.

**`stayConnectedMillis`** (hardcoded 3000ms, member of `BLEDimmer`, seeded from a
`mainLedstripController.cpp` local variable of the same name; was `keepOpenMillis` until
2026-07-19)
— *Relevant to: shape 1.*
How long a dimmer connection is kept open after a command, in case another command follows
immediately (avoids a full reconnect for rapid adjustments, e.g. dragging a brightness slider).
**Interplay:** directly trades off against the shared connection pool (see "Known limitation"
below) -- every extra second a connection is held open is a second it's not available for another
dimmer or an inbound server connection. Also interacts with `scanCooldownDiscoveryMillis`: while a
connection is held open, `updateScanning()` still won't start a new discovery scan for *other*
unaddressed dimmers, so a long `stayConnectedMillis` delays their discovery. **Deliberately left
hardcoded, not made configurable**, same reasoning and same #144 as `unreachableGiveUpMillis`
above -- this is also generic connection-lifecycle orchestration, not dimmer-specific.

## Known limitation: shared connection pool

`CONFIG_BT_NIMBLE_MAX_CONNECTIONS` defaults to 3, shared across outbound (client) and inbound
(server) connections. `control` can in principle want up to 4 simultaneous dimmer connections
plus any inbound server connection. Raising the cap is a blunt fix (more RAM, doesn't address why
several connections would need to be open at once) -- the better lever is keeping `stayConnectedMillis`
and the number of simultaneously-addressed-but-unconfirmed dimmers low enough that this rarely
matters, and letting the ENOMEM path (client side) or the advertising-retry path (server side,
fixed today) degrade gracefully when it does.
