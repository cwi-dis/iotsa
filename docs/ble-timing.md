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

**`scanDurationPresenceMillis`** (default 11000ms)
— *Relevant to: shape 1 mainly (you can observe the server's real advertise cadence).*
Upper bound for a *presence-check* scan -- confirming an already-addressed device is still
around. In practice this scan usually ends early (see `allKnownDevicesSeenSince()`), so this
value mostly matters as a safety ceiling: if a presence check never completes early, this is how
long the radio is tied up before giving up. Keep it well above the server's advertise interval,
or a live presence check is likely to time out even though the peer is fine.

**`scanCooldownDiscoveryMillis`** (default 4000ms) / **`scanCooldownPresenceMillis`** (default 4000ms)
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

### Client side, not yet configurable

**`SCAN_START_RETRY_MS`** (hardcoded 1000ms, `iotsaBLEClient.cpp`)
— *Relevant to: all three shapes.*
Retry delay when a scan-start attempt fails -- either because NimBLE rejected it (a connect was
in progress) or because our own `connectingCount > 0` guard held it off proactively. Too short
wastes CPU/log spam re-attempting during a connect that's known to still be running; too long
delays discovery of new/missing devices unnecessarily.

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

**`minAdvertisingDuration`** (hardcoded 70ms) / **`extraDurationForConnecting`** (hardcoded 30ms)
(`resumeServer()`, `iotsaBLEServer.cpp`)
— *Not a good fit for independent configurability.* These are derived from BLE physics at the
*default* advertising interval (70ms ≈ one full cycle at the 20ms minimum interval, plus margin).
If `adv_min` is changed from its default, these hardcoded values go stale. Should be computed
from `adv_min` rather than exposed as their own settings -- a correctness fix, not a new knob.

### lissabon side (`BLEDimmer`, `mainLedstripController.cpp` -- not in this repo, listed for
completeness since the interplay crosses the repo boundary)

**`discoveryTimeoutMillis`** (hardcoded 10000ms, member of `BLEDimmer`, `BLEDimmer.h`; was a
`BLEDimmer.cpp` file-scope constant misleadingly named `IOTSA_BLEDIMMER_CONNECT_TIMEOUT` until
2026-07-19)
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

**`keepOpenMillis`** (hardcoded 3000ms, `mainLedstripController.cpp`, already flagged
`// xxxjack should be configurable`)
— *Relevant to: shape 1.*
How long a dimmer connection is kept open after a command, in case another command follows
immediately (avoids a full reconnect for rapid adjustments, e.g. dragging a brightness slider).
**Interplay:** directly trades off against the shared connection pool (see "Known limitation"
below) -- every extra second a connection is held open is a second it's not available for another
dimmer or an inbound server connection. Also interacts with `scanCooldownDiscoveryMillis`: while a
connection is held open, `updateScanning()` still won't start a new discovery scan for *other*
unaddressed dimmers, so a long `keepOpenMillis` delays their discovery.

## Known limitation: shared connection pool

`CONFIG_BT_NIMBLE_MAX_CONNECTIONS` defaults to 3, shared across outbound (client) and inbound
(server) connections. `control` can in principle want up to 4 simultaneous dimmer connections
plus any inbound server connection. Raising the cap is a blunt fix (more RAM, doesn't address why
several connections would need to be open at once) -- the better lever is keeping `keepOpenMillis`
and the number of simultaneously-addressed-but-unconfirmed dimmers low enough that this rarely
matters, and letting the ENOMEM path (client side) or the advertising-retry path (server side,
fixed today) degrade gracefully when it does.
