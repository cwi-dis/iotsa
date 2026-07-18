# BLE power vs. responsiveness: a tuning guide

This is the plain-language companion to [`ble-timing.md`](ble-timing.md), which lists every
setting with its exact name, default, and source-verified interplay notes. Read this one first if
you're trying to *decide* what values to use for a real deployment; go to `ble-timing.md` when
you need the precise field name or a technical detail. For real measured power numbers on actual
hardware, see [lissabon's `doc/ble-power.md`](../../lissabon/doc/ble-power.md).

## The one idea that matters

Every BLE-powered iotsa device — server or client — spends most of its time with the radio mostly
off, and wakes it up briefly and repeatedly. **The less time the radio is on, the less power the
device uses, and the less often (or less reliably) it's actually reachable.** Every setting below
is a dial somewhere on that same line between "sips power, sometimes hard to reach" and "always
reachable, drains the battery."

There are two separate versions of this dial — one on the device being connected *to* (the
server, e.g. a lissabon ledstrip), and one on the device doing the connecting (the client, e.g.
`control`). Getting good behavior means understanding both dials, and — this is the part that
actually causes real bugs — making sure neither one is set so aggressively that it can't
physically meet the other in the middle.

## The server's dial: sleep / wake / advertise

A battery-powered iotsa server (a lissabon dimmer or ledstrip) spends its life in a repeating
cycle:

1. **Sleep.** Radio off, CPU mostly off. This is where almost all the power saving happens.
2. **Wake.** Radio on. While awake, it repeatedly **advertises** — sends out a short "I'm here"
   radio burst — so that a scanning client can find it and connect.
3. Back to sleep, repeat.

Three numbers control this:

- **How long it sleeps** each cycle (`sleepDuration`) — longer sleep = lower average power, but
  longer gaps where the device simply cannot be found or connected to.
- **How long it stays awake** each cycle (`wakeDuration`) — shorter wake = lower power, but a
  narrower window for a client to actually catch it during that wake period.
- **How often it advertises while awake** (`adv_min`/`adv_max`) — advertising less often during
  the wake window saves a little more power, but makes the window itself less "dense" — a client
  scanning during the right wake period could still miss it if the gaps between individual
  advertisement bursts are too wide relative to how long the client is willing to listen.

A real example from the fleet (verified 2026-07-19 against the actual deployed configs in
`lissabon-config/*/battery.json`, all consistent): the lissabon ledstrips and dimmers run
`sleepDuration=1500` (1.5 seconds) and `wakeDuration=300` (300 milliseconds) — a full
sleep+wake cycle of 1.8 seconds. That's a deliberately *tight* cycle, not an extreme one: it
trades away some of the power saving a much longer sleep could offer, in exchange for a device
that's reachable within a couple of seconds essentially any time someone wants it, rather than
making them wait for a rare wake window. (`control` itself is the exception here — it uses deep
sleep with `wakeDuration=60000`, a different mechanism, not the light-sleep cycle described in
this section.)

## The client's dial: scan duration / cooldown

A client (like `control`) doesn't advertise, it listens. Its cycle is the mirror image:

1. **Scan.** Radio on, actively listening for advertisements from devices it cares about.
2. **Cooldown.** Radio off (or at least not scanning), to save power and give the radio a break.
3. Repeat.

Two numbers control this (there's a separate pair for "hunting for a device we don't know the
address of yet" vs. "just confirming a known device is still there" — see `ble-timing.md` for the
exact names — the idea is the same for both):

- **How long each scan runs** (`scanDuration...`) — longer scans = more likely to catch a brief
  advertisement window somewhere in the middle, but more power spent listening, often to nothing.
- **How long the gap is between scans** (`scanCooldown...`) — shorter cooldown = scans happen
  more often = better odds of overlapping the server's wake window soon, but again, more power.

There's a third, related dial that only kicks in *after* the client has already found the device
and is trying to actually connect to it: **how long to wait for the connection itself to
establish** (`connectTimeoutMillis`). Too short and a connection that would have succeeded given
another second gets abandoned prematurely — indistinguishable, from the outside, from the
scan/sleep mismatch described below, but a completely different cause. Too long and the radio sits
tied up "trying to connect" for longer than useful (and, since connects now take priority over
scanning, that's also longer that scanning for *other* devices is held off) before giving up on a
genuinely unreachable device. There's no power tradeoff here the way there is for the other two —
it's purely about how patient to be with one connection attempt before trying again.

## Where it goes wrong: mismatched dials

First, the good news: the fleet's real numbers are actually well-matched. Pair the ledstrip's
real 1.5s-asleep / 300ms-awake cycle with `control`'s *default* client settings —
`scanDurationDiscoveryMillis=11000` (scans for 11 seconds), `scanCooldownDiscoveryMillis=4000` (waits 4
seconds between scans) — and one 11-second scan burst spans about six full 1.8-second server
cycles, i.e. about six separate 300ms wake windows to catch. The odds of missing all six in one
burst are low. This matches what was actually observed on real hardware this session: once
`control` could see `stripbank` by name at all, it caught an advertisement in nearly every single
scan cycle, not occasionally.

Here's the failure mode Jack flagged, illustrated with a **hypothetical** (not real) pairing, to
show what a genuine mismatch looks like. Suppose a rarely-touched device were tuned much more
aggressively for power — say `sleepDuration=60000` (1 minute) and `wakeDuration=100` (100ms), a
60.1-second cycle — while the client keeps its *default* discovery settings (11s scan / 4s
cooldown). Now one 11-second scan burst covers only about 11/60 ≈ 18% of one server cycle, so
most individual scan attempts won't contain any part of that 100ms window at all. The client
isn't broken and isn't scanning too rarely in absolute terms (73% duty cycle, same as before) —
it's just that the thing it's trying to catch happens on a clock roughly 30x slower than its own
scan-burst length, so a single attempt has a low chance of overlapping it. Expect discovery to
take several retries, spread over a minute or more, rather than succeeding on the first attempt.

**The rule of thumb:** if a client's scan duration is a healthy multiple of the server's full
sleep+wake cycle (as the real fleet numbers are — roughly 6x, in the example above), you're fine.
If the client's scan duration is *shorter than or comparable to* the server's cycle, you're
relying on the scan burst happening to line up with the wake window, and connections will feel
unreliable or slow to establish even though nothing is actually broken. When something "sometimes
connects, sometimes doesn't, no obvious pattern," this mismatch is the first thing to check —
before suspecting a bug.

**A second, unrelated cause of that exact same symptom:** `connectTimeoutMillis` set too low.
This isn't hypothetical -- it's what actually happened to `control` earlier today: a units bug
configured a 6-millisecond connect timeout instead of 6 seconds, so every single connect attempt
was abandoned essentially the instant it started, long before the link had any real chance to
establish. From the outside this looked identical to a scan/sleep mismatch ("can't connect,
no obvious pattern") even though scanning and sleep timing had nothing to do with it. If the
scan/sleep numbers check out and connects are still flaky, check `connectTimeoutMillis` next.

This is exactly the shape of thing that's easy to configure independently on each device and
then forget you did, since the server owner and the client owner might not even be the same
config session, or the same day. If you change a server's sleep/wake cycle, go check whether any
client that talks to it still has a sane chance of catching it — and vice versa.

## Practical guidance

- **Battery-powered, rarely-touched devices** (most lissabon ledstrips/dimmers): the fleet's real
  1.5s/300ms cycle is a good reference point — tight enough to feel reachable within a couple of
  seconds, without the client needing unusually long scan bursts to compensate. Going much longer
  on sleep saves more power, but only pays off if you also lengthen the client's scan duration to
  match (see the rule of thumb above) — otherwise you're trading power for unreliability, not for
  a predictable slower-but-working tradeoff.
- **Devices someone is actively interacting with right now** (e.g. `control` while someone's at
  the touchpad adjusting brightness): once a connection is established, `keepOpenMillis` keeps it
  open briefly so a rapid follow-up command doesn't pay the full discovery+connect cost again —
  this matters more for felt responsiveness than either sleep or scan tuning once you're already
  connected.
- **Mains-powered or otherwise power-unconstrained devices**: there's little reason not to
  shorten sleep/lengthen wake (server) or shorten cooldown (client) — the power tradeoff barely
  applies, so lean toward "reachable" over "efficient."
- **When in doubt, check the numbers before suspecting a bug.** A connect failure that "sometimes
  works" is much more likely to be a sleep/scan mismatch or a too-low `connectTimeoutMillis` than
  an actual bug — check both before assuming something's broken.
