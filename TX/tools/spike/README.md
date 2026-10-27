# BLE coexistence spike (#19)

One-day go / no-go for the iOS app (#17): can the TX run its WiFi AP, web server, LittleFS, RF24 **and** a BLE GATT service without falling over or hurting the radio link? Design background: `iOS/DESIGN.md` section 3.2 and section 7, risks 1 and 2.

Everything here is throwaway. If the answer is no-go, delete this folder, the two `spike-*` envs in `TX/platformio.ini` and the `#ifdef SPIKE` / `#ifdef BLE_SPIKE` blocks in `TX/src/main.cpp`. The normal build (`pio run`, env `az-delivery-devkit-v4`) has none of it compiled in.

## Builds

| Env | What it is |
| --- | --- |
| `spike-baseline` | Normal firmware + heap telemetry + RF test. BLE not compiled in. |
| `spike-ble` | The same + NimBLE-Arduino with the GATT service below. |
| `spike-ble-only` | `spike-ble` with `-DSPIKE_NO_WIFI`: no WiFi AP, DNS or web server at all. Ad hoc, added to isolate a connection-reliability problem (see Results). |
| `spike-ble-debug` | `spike-ble-only` plus `CONFIG_NIMBLE_CPP_LOG_LEVEL=4` and `CORE_DEBUG_LEVEL=5`. This is what finally root-caused the advertising bug — reach for it early rather than guessing. |
| `spike-ble-next` | `spike-ble-only` on espressif32 8.0.0. **Does not build** — kept deliberately to document why (see the platform bump section). |

`tools/spike/ble_min/` is a separate standalone PlatformIO project: a minimal NimBLE peripheral with no WiFi or web server, for isolating BLE behaviour from the rest of the firmware and for testing platform/library versions. It is the cleanest reproduction of the advertising bug and the right place to try alternatives.

Flash with `pio run -e spike-ble -t upload` (and `-t uploadfs` once for the web UI, as usual). CI builds the first three envs (not `spike-ble-only`) and lists their RAM and flash use in the job summary.

GATT service `9A9476F2-AE0D-406A-847A-D0FECAF34CE5`, advertised as `Drum Lights TX` (override with `#define BLE_NAME` in `secrets.h`):

| Characteristic | UUID | Properties | Value |
| --- | --- | --- | --- |
| State | `E1EBAF93-...6FDE` | read, notify | `int32 mode` LE + `uint8 flags` (bit 0 = auto) |
| Command | `53F118A7-...8219` | write | `int32 mode` LE, e.g. `01000000` = green, `62000000` = strobe, `FFFFFFFF` = auto |
| Info | `B1AB44F5-...8427` | read | `{"proto":1,"fw":"...","catalogue":1}` |

No passkey or bonding in the spike, so anyone nearby can write to it. Phase 2 adds that.

## TX serial output

At boot: `SPIKE boot reset=<esp_reset_reason> sketch=<bytes> freesketch=<bytes> ...`. A reset reason other than 1 (power on) during a test means it crashed or the watchdog fired.

Every 10 s: `SPIKE t=... heap=... minheap=... maxblock=... ws=... wsCmd=... wsDisc=... ble=... bleConn=... bleCmd=... applied=...`

Serial commands (type in `pio device monitor`): `r` starts the RF test, `x` stops it, `t` prints telemetry now. On BLE builds, `a` reports the advertising state and tries `start()`, `b` does `stop()`+`start()` — both added to diagnose the advertising bug below, and worth keeping while it is unresolved.

## 1. Heap and flash

1. Flash `spike-baseline`, connect one phone to the web UI, wait a minute, note `heap`, `minheap`, `maxblock`.
2. Flash `spike-ble`, connect the web UI and nRF Connect (subscribed to State), note the same.
3. Flash use comes from the CI job summary (or `sketch` / `freesketch` in the boot line).

Rough bar for go: `minheap` stays above ~40 KB with both clients connected, and `maxblock` above ~20 KB (AsyncWebServer allocates per request).

## 2. Soak, 30 minutes

`spike-ble` flashed, TX serial log captured (`pio device monitor | tee tx-soak.log`).

```sh
pip install websockets bleak
python soak.py --ws --ble --minutes 30
```

Optionally have a phone on the web UI tapping buttons as well. Go if: no resets in the TX log, `minheap` flat (not trending down), `ws_drops` and `ble_drops` at 0 (or explained), `ble_errors` at 0.

## 3. RF check

Two or three receivers on the bench, each logging serial: in `RX/`, `pio device monitor > rx1.log` (one per board, e.g. with `--port`).

1. Flash `spike-baseline` on the TX, type `r`, wait for `RFTEST done` (about 80 s). Save logs as `off-rx1.log` etc.
2. Flash `spike-ble`, connect nRF Connect and subscribe to State notifications (so BLE carries traffic during the test), type `r`, save logs as `on-rx1.log` etc.
3. Score: `python rf_score.py off-*.log on-*.log`

The test alternates strobe 98 with solid colours 1 to 8 every 400 ms for 200 steps, 5 packets per step. Go if missed changes and packet loss with BLE on are no worse than with it off (allow a point or two of packet loss for noise; run each twice if it's close).

## Results

Build sizes from CI (espressif32 7.1.3, NimBLE-Arduino 2.5.2, default 4 MB partition table with a 1,310,720-byte app slot):

| Env | Flash | Static RAM |
| --- | --- | --- |
| `az-delivery-devkit-v4` | 876,021 (66.8%) | 45,888 |
| `spike-baseline` | 878,417 (67.0%) | 45,936 |
| `spike-ble` | 1,132,453 (86.4%) | 55,672 |

BLE costs about 254 KB of flash and 10 KB of static RAM, leaving about 178 KB in the app slot. Compiling out the central and observer roles saved only 13 KB. If phase 2 runs short of flash, switch to `board_build.partitions = huge_app.csv` (3 MB app, 896 KB filesystem, no OTA, which the TX doesn't use). `min_spiffs.csv` won't do: the built web UI is about 588 KB and that layout's filesystem is much smaller. Static RAM is not the real test: NimBLE allocates its controller and host memory at `init()`, so the heap numbers from step 1 decide it.

### Three firmware bugs found and fixed on the bench

None were caught by CI (all build clean) — they only showed up flashed to real hardware:

1. **The GATT server was never started.** `NimBLEServer::start()` (`ble_gatts_start()`) was never called, so the State/Command/Info characteristics were never registered with the NimBLE host. Advertising ran regardless, so the boot log looked fine, but nothing could discover the service. Fixed in `2dc254b`.
2. **Advertising didn't resume after a disconnect.** `NimBLEServer::advertiseOnDisconnect` defaults to `false` in NimBLE-Arduino 2.x. After the first BLE client disconnected, the TX went permanently invisible to every scanner until the next reboot. Fixed in `6c89d6a`.
3. **Advertising didn't resume after an *aborted connection attempt*** — a different path to the same symptom, with a different cause, and the big one. See the section below; it needed the stale-host-flag workaround, not a config flag.

1 and 2 confirmed fixed by connecting, disconnecting, and rescanning. 3 is worked around rather than fixed.

### 1. Heap and flash — partially done

Numbers gathered on the bench (`spike-ble` unless noted):

| Scenario | heap | minheap | maxblock |
| --- | --- | --- | --- |
| `spike-baseline`, phone on web UI | 183,200 | 124,912 | 110,580 |
| `spike-ble`, idle (no clients) | 85,368 | 83,480–83,788 | 65,524 |
| `spike-ble`, phone on web UI only | 81,148–83,380 | 24,212–31,800 | 18,420–55,284 |
| `spike-ble`, BLE client only (no WiFi AP, see below) | 221,144 | 219,616 | 110,580 |
| `spike-ble`, BLE client only (WiFi AP running, no WS client) | 85,384 | 83,676 | 65,524 |

Not obtained: a true simultaneous WS + BLE reading. Every attempt to hold both at once ran into the advertising bug below before a steady state was reached. That bug now has a workaround, so **this step should be straightforward to finish next session** — connect the web UI and a BLE client, then read `t`. The WS-only `minheap`/`maxblock` numbers above vary run to run and one sample dipped to 24,212/18,420 — below the ~40 KB/~20 KB rough bar this doc set earlier — worth another look once BLE connects reliably enough to add to that figure.

### The main finding: an aborted connection attempt kills advertising

**Root-caused, worked around, and the workaround is in `spike-ble`. Read this first next session.**

A BLE connection attempt that a central starts and then abandons mid-handshake stops the TX advertising. Before the workaround it stayed dead until reboot, invisible to every scanner.

Exactly matches a known upstream report: [project-chip/connectedhomeip#74202](https://github.com/project-chip/connectedhomeip/issues/74202) — "a central device cancels a connection while the ESP32 peripheral is still reading the remote device's version and features". That issue is in Matter's own application code (it failed to check `connect.status`), so its fix doesn't apply to us, but it documents the NimBLE behaviour: a failed setup arrives as `BLE_GAP_EVENT_CONNECT` with non-zero status, not as a disconnect.

**Our case is worse than that, and the mechanism is specific.** With `CONFIG_NIMBLE_CPP_LOG_LEVEL=4` and `CORE_DEBUG_LEVEL=5` (env `spike-ble-debug`), an aborted attempt logs **nothing at all** — no GAP event reaches the host. `NimBLEServer::handleGapEvent` logs on entry at D level and other D-level lines appear fine, so the handler genuinely never runs. That rules out both candidate paths in NimBLE-Arduino's handler (the `rc != 0` branch, which would log and restart advertising, and the `ble_gap_conn_find` early-return on line ~460 of `NimBLEServer.cpp`). The fault is below the host, in the controller or HCI transport.

The decisive measurement, via a serial `a` command added for it:

| | `isAdvertising()` | `start()` result |
| --- | --- | --- |
| Healthy (discoverable) | `1` | "Advertising already active" |
| Broken (invisible to all scanners) | `1` | "Advertising already active" |

**The host's advertising flag goes stale.** It keeps reporting `true` after the controller has stopped, so `start()` short-circuits as a no-op and the application cannot recover through the normal API. That is why everything tried earlier in the session failed.

**Workaround (verified):** call `stop()` *first* — which clears the stale host flag — then `start()`, which then actually reaches the controller. Verified end to end: healthy → FOUND, abort → NOT VISIBLE, `stop()`+`start()` → FOUND again, no reboot.

Since there is no event to trigger on, `rearmAdvertising()` in `src/main.cpp` does this blind on a timer (`ADV_REARM_TIME`, 10 s) whenever no client is connected.

### Measured connection reliability (`ble_reliability.py`)

`tools/spike/ble_reliability.py` scores connect → subscribe → hold → disconnect cycles, scanning fresh each time. All runs below are 10 cycles from the same Mac, immediately after flashing:

| Build | Re-arm interval | Success | Median connect |
| --- | --- | --- | --- |
| `spike-ble` (before the workaround) | — | **0/20** | — |
| `spike-ble` (WiFi AP running) | 10 s | **8/10** | 4.4 s |
| `spike-ble-only` (WiFi compiled out) | 10 s | 6/10 | 3.9 s |
| `spike-ble-only` | 60 s | 2/10 | 7.0 s |
| `spike-ble-only` | 2 s | 8/10 | 4.2 s |

Three things fall out of that table:

1. **The workaround is the difference between unusable and mostly working.** 0/20 before: cycle 1 failed to connect, and cycles 2-20 then all failed at the *scan* stage because the first failure had already killed advertising. Failures are now isolated instead of permanent.
2. **WiFi is not the cause.** 6/10 with WiFi compiled out versus 8/10 with the AP running — no better, and the same within noise at this sample size. **This overturns the WiFi/BT coexistence explanation** that the first half of the session was built on. Those observations were real but misattributed: what actually happened was that an early failure killed advertising permanently, so everything after it failed for that reason rather than from contention. Risk 1 in `iOS/DESIGN.md` section 7 is *not* substantiated by this spike.
3. **The re-arm interval is load-bearing, and faster is better up to a point.** 60 s is much worse (2/10, and scan failures reappear) because the TX sits in a dead window between re-arms. 2 s is no better than 10 s. An earlier worry that the watchdog might clip connection attempts in flight was tested and is wrong — the opposite is true.

**What is still unexplained:** a residual ~20% connect failure rate and multi-second connect latency (median ~4 s, worst 15.8 s). BLE connect should be well under a second. This is not WiFi and not the re-arm interval, so it is either the ESP32 controller itself or the test client.

### Measured from an actual iPhone — same result, so it is the ESP32

The figures above came from one macOS CoreBluetooth client, which left open the possibility that macOS was the flaky part. It isn't. Ten manual connect/disconnect cycles from an iPhone in LightBlue, against `spike-ble` (WiFi AP running, re-arm watchdog active), with the TX's serial captured throughout:

| | iPhone (manual, 10 cycles) | Mac (`ble_reliability.py`, 10 cycles) |
| --- | --- | --- |
| Connected first try | **8/10** | 8/10 |
| Connect latency | 4-10 s, reported as "subjectively too slow" | median 4.4 s, max 10.2 s |

Essentially identical. **The connection-establishment problem is in the ESP32, not the test client.** That closes off the "maybe it's just macOS" hypothesis, which was the main reason to doubt the earlier numbers.

Supporting detail from the TX log for that run:

- `bleConn=8` — exactly 8 connections reached the host, matching the 8 the operator saw succeed. The 2 failures never reached the TX at all, consistent with the abort path described above.
- **Two of the 8 "successes" ended in `reason 520`**, which is `BLE_HS_ERR_HCI_BASE` (0x200) + `BLE_ERR_CONN_SPVN_TMO` (0x08) — a *connection supervision timeout*, i.e. the link died on its own rather than being disconnected. One lasted only 3 seconds. The other 6 ended in `reason 531` (`BLE_ERR_REM_USER_CONN_TERM`), the normal operator-initiated disconnect. So established links were not uniformly clean either, which the 30-minute soak (one long-lived connection) did not surface.
- One boot, no resets. Heap flat at 85,364 with `minheap` 83,476 throughout — memory is a non-issue.
- Two different client addresses appear (`54:ae:...` and `58:8b:...`) for the same phone: iOS BLE address randomisation, expected, and worth remembering for phase 2 bonding.

**What this means for the go/no-go.** The thing the app depends on most — an operator reliably and quickly getting connected — measures at 80% first-try and 4-10 seconds on the real target platform, with occasional spontaneous link loss. For a gig that is not good enough: waiting ~5 seconds and retrying one time in five while a set is running is exactly the failure mode the app was supposed to remove. Everything *else* passes (flash, RAM, link stability once established, no crashes), so this is a narrow but decisive blocker rather than a broad one.

It is also now clearly an upstream/platform problem rather than something to fix in this firmware: reproducible in a 90-line sketch with no WiFi (`ble_min`), unchanged by a platform major bump, and invisible to the application API.

**Ruled out:** `esp_coex_preference_set(ESP_COEX_PREFER_BT)` in `initBLE()`. Call succeeds (`rc=0`), changes nothing. Left in as harmless. Note it was aimed at the coexistence theory that point 2 above has since undermined.

### Platform bump (espressif32 8.0.0) — tried, does not fix it

Two separate results, and both matter.

**The main TX firmware cannot be built on 8.0.0 at all.** 8.0.0 brings Arduino-ESP32 3.x (vs 2.x on 7.1.3), which drops `IPv6Address.h`; `AsyncTCP-esphome` still includes it, so the build fails outright. This is exactly the "ipv6 issues" that the production pin in `platformio.ini` exists to avoid — leave that pin alone. Moving the TX to a newer platform needs the web stack dealt with first, which is its own piece of work and nothing to do with BLE: tracked in #41. Note #19 is *not* waiting on it, since the bump does not fix the BLE bug anyway (below).

**The BLE bug is still present on 8.0.0.** Since that build failure is in the web stack, `tools/spike/ble_min/` was added to test the BLE question independently: a standalone PlatformIO project with a minimal NimBLE peripheral and nothing else — no WiFi, no web server, no re-arm watchdog — using the same service UUIDs so `ble_reliability.py` works against it unchanged. It builds cleanly on both platforms.

| Platform | `ble_min` result | Signature |
| --- | --- | --- |
| 7.1.3 | 2/10 | cycles 3-4 fail to connect, then 5-10 all fail at *scan* |
| 8.0.0 | 6/10 | cycle 7 fails to connect, then 8-10 all fail at *scan* |

The success counts differ only by *when* the first aborted attempt happened to land; the signature is identical, and in both cases advertising stays dead for every subsequent cycle. Median connect on 8.0.0 was 5.6 s, no better than 7.1.3. **Not fixed by the bump.**

Two things learned along the way:

- **`ble_min` is a clean minimal reproduction** — pure NimBLE, ~90 lines, no WiFi. That rules out our firmware's complexity entirely and makes this suitable to file upstream as-is.
- **The bug is intermittent per abort.** One deliberate abort left advertising healthy; the reliability runs show it firing after a few cycles. So "abort kills advertising" is probabilistic, not every time — which is why early manual testing gave such confusing, inconsistent results.

### Faster advertising interval — tried, not a fix

`setMinInterval(32)` / `setMaxInterval(48)` in `initBLE()` — 20-30 ms, the floor for connectable undirected advertising, against NimBLE's slower default. The theory was that more advertising events per second gives a central more chances to land its connection request, attacking both the failure rate and the latency. Overridable via `ADV_MIN_INTERVAL` / `ADV_MAX_INTERVAL` to A/B it.

| Run | Success | Median connect |
| --- | --- | --- |
| Baseline (default interval), 10 cycles | 8/10 (80%) | 4.4 s |
| Fast interval, 10 cycles | 9/10 (90%) | 3.3 s |
| **Fast interval, 20 cycles** | **14/20 (70%)** | **3.0 s** |

**Not a fix.** The first 10-cycle run looked like a clear win at 9/10 and it was simply luck — doubling the sample put it at 70%, i.e. indistinguishable from the 80% baseline. Worth remembering when judging any future candidate fix here: **10 cycles is not enough to tell 70% from 90%, so run at least 20 before believing a result.**

Latency may be genuinely slightly better (median 4.4 s → 3.0 s across the larger sample), which is a real if small gain, and there is no downside to advertising faster for a peripheral that wants to be found quickly. The change is therefore kept. But 3 s and 70% is not usable, so this does not change the picture.

**Worth trying next, in order:**

1. ~~Measure from a real iPhone~~ — **done, see above.** Same 80% / 4-10 s as the Mac, so the fault is the ESP32.
2. ~~Faster advertising interval~~ — **done, see above.** Marginal latency gain, no reliability gain.
3. File it upstream against NimBLE-Arduino and/or esp-idf, using `ble_min` as the reproduction: an aborted connection attempt can leave the controller not advertising while the host's `isAdvertising()` still returns true, with no GAP event raised, so `start()` no-ops and only `stop()`+`start()` recovers. Reproduced on espressif32 7.1.3 and 8.0.0, NimBLE-Arduino 2.5.2. Related but not identical: [project-chip/connectedhomeip#74202](https://github.com/project-chip/connectedhomeip/issues/74202).
3. Try a different NimBLE-Arduino version against `ble_min` (cheap now that the harness exists — just change `lib_deps`).
4. Faster advertising interval (`setMinInterval`/`setMaxInterval`, ~20 ms) — more advertising events means more chances for a connection request to land, which may cut both the abort rate and the connect latency. Probably the best lead for the residual ~20%.
5. Swap the host stack as a *diagnostic*: on ESP32 the only real alternative to NimBLE is **Bluedroid** (stock ESP-IDF, behind Arduino's `BLEDevice`). Other "BLE libraries" for Arduino-ESP32 wrap one of those two, so they are not an independent test. Bluedroid costs significantly more flash and RAM — the reason the design doc chose NimBLE — and `spike-ble` is already at 86.5% flash, so it would need `huge_app.csv`. `ble_min` is the place to try it, not the main firmware. Only worth it to answer "is this NimBLE or the shared controller?": if the bug reproduces under Bluedroid too, no library swap will help.

### 2. Soak, 30 minutes — done, go (for a connection already established)

Run on the bench: `spike-ble` flashed, `soak.py --ble --minutes 30` from a Mac (no `--ws`; see below), TX serial captured in parallel.

- One reconnect at t=0 (scanner caught the TX before advertising had settled; reconnected in 7 s), then one unbroken connection for the remaining ~29.5 minutes.
- `ble_sent=3155 ble_state=3155 ble_drops=1 ble_errors=0` (client side); TX serial shows `bleCmd=3155 applied=3155` — every command the client sent was received and applied, zero loss end to end.
- `minheap` fell from 83,772 to 65,280 over the first ~9 minutes, then held *exactly* flat (identical to the byte) for the remaining ~21 minutes. A real leak would still be drifting down at minute 30; it wasn't. `maxblock` likewise flat at 59,380 from the same point.
- One boot line for the whole run (no resets), no panics, no watchdog triggers in the serial log.

Worth noting given the finding above: this soak's one connection happened to land on the first retry (7 s), and then never had to reconnect for 30 minutes. It doesn't contradict the connection-reliability problem — it just never needed to establish a second connection while WiFi was active.

Not yet run: `--ws` concurrently with `--ble`. `soak.py --ws` requires the test machine to join the TX's WiFi AP, which has no internet of its own — doing that from the machine running Claude Code would cut the session's own network connection for the duration, so this run used `--ble` only. To cover the WS+BLE combination, either run `soak.py --ws --ble` from a machine that isn't also driving this session, or run `--ws` from a second device at the same time as this `--ble` run.

### 3. RF check — not yet run

Was blocked on the advertising bug (the BLE-on half needs a client connected and subscribed for the full 80 s). Now that the workaround holds a connection reliably once established, this should be runnable as written. The BLE-off half (`spike-baseline`) needs no BLE client at all and can be banked any time.

Record in #19: heap and flash numbers for both builds, the soak summary line, the `rf_score.py` table, and the verdict.

### Answered on an ESP32-S3 — it connects reliably and sub-second

**This is the hardware question the Recommendation below asked for, and the answer is yes.**

Measured 2026-10-27 on a bare ESP32-S3 (16 MB flash, MAC `98:c3:77:a1:9d:94`, espressif32 7.1.3, NimBLE-Arduino 2.5.2), from the same Mac that produced the TX figures above. The S3 ran a throwaway harness carrying the TX's full load — WiFi AP on channel 6, captive DNS, AsyncWebServer, WebSocket, LittleFS, the same GATT service and the same one-command-queue-on-the-loop-task design — with the RF24 send simulated by an equivalent blocking delay. It has since been deleted as disposable; recover it with `git checkout 7159624 -- TX/tools/spike/harness` if these runs need repeating.

| Build | Success | Median connect | Max |
| --- | --- | --- | --- |
| TX `spike-ble` (classic ESP32) | 8/10 | 4.4 s | 10.2 s |
| S3, re-arm watchdog on | **20/20** | **1.01 s** | 1.08 s |
| S3, re-arm watchdog **off** | **20/20** | **1.03 s** | 1.15 s |

Forty cycles, no failures, every connect between 0.96 and 1.15 s. All 40 disconnects were `reason 531` (`BLE_ERR_REM_USER_CONN_TERM`, the normal client-initiated close) — **no supervision timeouts**, against 2 spontaneous link losses in 8 connections on the TX's iPhone run.

**The watchdog-off column is what settles it.** On the classic ESP32, disabling the stop+start re-arm gives 0/20, with advertising permanently dead after the first aborted connect and every later cycle failing at the *scan* stage. The S3 does not notice: 20/20 either way, with `rearm: -1` in the telemetry confirming it was off. **The S3 does not have the aborted-connect advertising bug** — that whole finding is specific to the classic ESP32's controller. Keep the workaround anyway: it is harmless, and the TX still needs it.

Heap headroom is also roughly doubled:

| | TX `spike-ble` | S3 |
| --- | --- | --- |
| Free heap after setup | 85,368 | **150,176** |
| `minheap`, idle | ~83,500 | 148,660 |
| `maxblock` | 65,524 | 139,252 |

The ~40 KB `minheap` / ~20 KB `maxblock` bar set earlier in this doc is cleared comfortably; the worst transient across the whole soak was 110,672 / 118,772.

Two caveats. The runs are **macOS only** — on the TX, Mac and iPhone agreed closely (8/10 at 4.4 s vs 8/10 at 4-10 s), so this probably carries over, but an iPhone run on an S3 is the check still outstanding before committing to the chip. And the ~1.0 s floor is so consistent across all 40 cycles that it is almost certainly CoreBluetooth's own connection-setup cost rather than anything the firmware controls; do not expect to tune it lower.

### The S3 soak found a crash — and it is ours, not the chip's

The S3 soak ran BLE commands every 500 ms *and* a phone on the web UI with its hammer enabled: the simultaneous WebSocket + BLE load the TX runs never achieved. It panicked after about 4 minutes.

```
ERROR: Too many messages queued          (213 times over the run)
WS client #1 disconnected
assert failed: tcp_sent IDF/components/lwip/lwip/src/core/tcp.c:2131
                        (invalid socket state for sent callback)
  AsyncClient::_error()    AsyncTCP.cpp:904
  AsyncClient::_s_error()  AsyncTCP.cpp:1307
  _async_service_task()    AsyncTCP.cpp:199
rst:0xc (RTC_SW_CPU_RST)
```

Mechanism: every mode change calls `notifyAll()` → `ws.textAll()`. At roughly 5 broadcasts/second to two WebSocket clients, `AsyncWebSocket`'s per-client queue overflowed (the 213 errors, each one a discarded message). A client then dropped, lwIP delivered a TCP error, and `AsyncClient::_error()` called `tcp_sent()` on a pcb lwIP had already freed when it raised that error — which lwIP's API forbids. Assert, panic, reboot.

**This is an `AsyncTCP-esphome` bug: not BLE, not coexistence, not S3-specific.** The production TX runs the same `AsyncTCP-esphome` / `ESPAsyncWebServer-esphome` and the same broadcast-on-every-change pattern, so **the TX has this latent crash today, with or without BLE.** The TX's own soak missed it only because it ran BLE-only with no WebSocket client attached. Normal TX use is gentler than this test — 5 broadcasts/s to 2 clients is well above an operator tapping buttons — but strobe-heavy use with the web UI open is the same shape of load, and the failure reboots the transmitter mid-set. The fix belongs in our code (rate-limit or coalesce `notifyAll()`, and check `availableForWriteAll()` before broadcasting) rather than waiting on upstream.

Everything else in that 15-minute run was clean, including the recovery:

| | |
| --- | --- |
| Panics / reboots | 1 at ~4 min; recovered in ~4 s unaided, BLE client reconnected itself |
| BLE commands | 1,651 applied for 1,650 sent across the reboot — none lost |
| Modes applied | 3,032 |
| `queueDrops` (our own mode queue) | 0 |
| BLE disconnects | 0 (the link only dropped via the reboot) |
| WS client disconnects | 4 (queue-overflow casualties) |
| Free heap at 11 min | 144,984 — flat from 145,180 at 10 s |

Free heap flat over 11 minutes is the no-leak evidence. `minheap` stepped down over the run (150,020 → 110,672), but that is a low-water mark and can only ever decrease, so its steps record transient peaks — the WebSocket reconnect bursts — not a leak.

### Still not covered after the S3 runs

- **RF24 interference (risk 2):** needs the real TX and receivers.
- **iPhone connect reliability on an S3:** see the caveat above.
- **A clean WS + BLE soak:** blocked by the `AsyncTCP` crash. Worth rerunning once `notifyAll()` is rate-limited, to confirm nothing else is hiding behind it.

A practical note for anyone repeating this on an S3 dev board: DTR drives GPIO0 there, so any serial tool that asserts DTR on open leaves the chip strapped into download mode until it is power-cycled — several flashing attempts were lost to that before it was understood. Open the port with DTR and RTS both deasserted and leave them that way on close.

**Where the go/no-go stands.** Everything the spike originally set out to measure passes: flash and RAM fit with headroom, the heap is flat, there are no crashes or resets *in the BLE-only soak*, and a BLE link once established carried 3,155 commands over 30 minutes with zero loss. (The later WS + BLE run on the S3 did crash, in `AsyncTCP` — see above. That applies to the TX as well, but it is not a BLE or coexistence failure.) Risk 1 (WiFi/BT coexistence) is **not substantiated** — removing WiFi entirely did not improve BLE.

The blocker is narrower and was not on the original list: **BLE connection establishment on the ESP32.** Measured on the real target platform (iPhone), it is 80% first-try at 4-10 seconds, with occasional spontaneous link loss. That is the one thing the app most depends on, and at those numbers an operator would be waiting ~5 seconds and retrying one time in five, mid-set — the exact experience the app exists to remove.

It is an upstream/platform problem, not a bug in this firmware: it reproduces in a 90-line NimBLE sketch with no WiFi, survives a platform major-version bump, and raises no event the application can see or recover from.

**Recommendation (superseded by the S3 result above — kept for the reasoning).** The cheap software leads are now exhausted: the coexistence fix, the platform bump and the advertising interval all failed to move connection reliability off ~70-80% at multi-second latency, and it reproduces in a 90-line sketch with no WiFi. One hardware question remains and it is worth answering before deciding: **run `ble_min` on an ESP32-S3 or C3.** It is close to a drop-in for that test, and the receivers, RF24 link and mode protocol are all transport-agnostic, so even a full TX change would not touch the fleet.

- If an S3 connects reliably and sub-second, the app is worth building and the TX just needs a newer chip.
- If it behaves the same, then BLE-on-ESP32 cannot carry a live performance tool, and the honest call is the design doc's no-go branch: put the effort into the web UI (phase 0). Note that would be for a completely different reason than the design doc anticipated — connection setup, not coexistence, which this spike found to be a non-issue.

Either way, don't spend more time on application-level workarounds in this firmware. The remaining uncertainty is in the silicon and the vendor stack, not here.

**Updated recommendation after the S3 run: go, on an S3.** The first branch above is the one that happened. BLE connection establishment — the sole blocker — measures 20/20 at ~1.0 s on an S3 under the full TX load, against 8/10 at 4.4 s on the classic ESP32, and the advertising bug is absent. Before committing: confirm with an iPhone, run the RF check, and fix the `AsyncTCP` crash (which is a TX bug today regardless of what happens to #19).

Also raised but not decided: dropping WiFi from the TX entirely and making BLE the only transport, with Android served by Web Bluetooth from a hosted copy of the web UI (`iOS/DESIGN.md` section 6). Note the measurements above weaken the main technical argument for it — removing WiFi did *not* improve BLE, so this is now a product decision (one transport to maintain, no hotspot-joining) rather than a fix for anything. The cost stays the same: no zero-setup fallback that works on any phone. An #17 decision, not a spike outcome.
