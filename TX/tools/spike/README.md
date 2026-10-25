# BLE coexistence spike (#19)

One-day go / no-go for the iOS app (#17): can the TX run its WiFi AP, web server, LittleFS, RF24 **and** a BLE GATT service without falling over or hurting the radio link? Design background: `iOS/DESIGN.md` section 3.2 and section 7, risks 1 and 2.

Everything here is throwaway. If the answer is no-go, delete this folder, the two `spike-*` envs in `TX/platformio.ini` and the `#ifdef SPIKE` / `#ifdef BLE_SPIKE` blocks in `TX/src/main.cpp`. The normal build (`pio run`, env `az-delivery-devkit-v4`) has none of it compiled in.

## Builds

| Env | What it is |
| --- | --- |
| `spike-baseline` | Normal firmware + heap telemetry + RF test. BLE not compiled in. |
| `spike-ble` | The same + NimBLE-Arduino with the GATT service below. |
| `spike-ble-only` | `spike-ble` with `-DSPIKE_NO_WIFI`: no WiFi AP, DNS or web server at all. Ad hoc, added mid-session to isolate a connection-reliability problem (see Results) — not part of the original plan, delete along with the rest of the spike. |

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

Serial commands (type in `pio device monitor`): `r` starts the RF test, `x` stops it, `t` prints telemetry now.

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

### Two firmware bugs found and fixed on the bench

Neither was caught by CI (both build clean) — only showed up flashed to real hardware:

1. **The GATT server was never started.** `NimBLEServer::start()` (`ble_gatts_start()`) was never called, so the State/Command/Info characteristics were never registered with the NimBLE host. Advertising ran regardless, so the boot log looked fine, but nothing could discover the service. Fixed in `2dc254b`.
2. **Advertising didn't resume after a disconnect.** `NimBLEServer::advertiseOnDisconnect` defaults to `false` in NimBLE-Arduino 2.x. After the first BLE client disconnected, the TX went permanently invisible to every scanner until the next reboot. Fixed in `6c89d6a`.

Both confirmed fixed by connecting, disconnecting, and rescanning afterward.

### 1. Heap and flash — partially done

Numbers gathered on the bench (`spike-ble` unless noted):

| Scenario | heap | minheap | maxblock |
| --- | --- | --- | --- |
| `spike-baseline`, phone on web UI | 183,200 | 124,912 | 110,580 |
| `spike-ble`, idle (no clients) | 85,368 | 83,480–83,788 | 65,524 |
| `spike-ble`, phone on web UI only | 81,148–83,380 | 24,212–31,800 | 18,420–55,284 |
| `spike-ble`, BLE client only (no WiFi AP, see below) | 221,144 | 219,616 | 110,580 |
| `spike-ble`, BLE client only (WiFi AP running, no WS client) | 85,384 | 83,676 | 65,524 |

Not obtained: a true simultaneous WS + BLE reading. Every attempt to hold both at once ran into the advertising bug below before a steady state was reached — this step is blocked on that, not on anything about heap itself. The WS-only `minheap`/`maxblock` numbers above vary run to run and one sample dipped to 24,212/18,420 — below the ~40 KB/~20 KB rough bar this doc set earlier — worth another look once BLE connects reliably enough to add to that figure.

### The main finding: an aborted connection attempt kills advertising until reboot

**This is the headline result of the session and the thing to pick up first next time.**

A single BLE connection attempt that is *started and then abandoned mid-handshake* permanently stops the TX advertising. It becomes invisible to every scanner — phone, Mac, any app — and only a reboot (or power cycle) brings it back. No WiFi involvement needed, no successful connection needed first.

Reproduced deliberately on `spike-ble-only` (WiFi entirely compiled out), from a Mac:

1. Scan, find `Drum Lights TX`, start connecting, abort after 3 s (`asyncio.timeout(3)` around the bleak connect).
2. Rescan: gone. Stayed gone across 19 consecutive further scan attempts.
3. Reboot: back immediately.

`tools/spike/ble_reliability.py` reproduces this as a side effect — it scores connect/disconnect cycles, and run against the current firmware it reports **0/20 success**: cycle 1 fails with a connect timeout, and cycles 2-20 then all fail at the *scan* stage because the first failure already killed advertising. That script is the fastest way to confirm whether a candidate fix works; a fix means 20/20, not 1/20.

**What makes this hard:** the TX logs *nothing* for the aborted attempt. No connect callback, no disconnect callback, no error, `bleConn` stays 0, heap untouched. The application code has no event to react to, so it cannot detect or recover from the condition through the NimBLE API. That points below NimBLE, at the ESP32 BLE controller getting stuck in a half-open "connecting" state that it never leaves.

**This supersedes the WiFi coexistence theory** that the earlier part of the session was built around. The observations below are still accurate as *observations*, but the explanation was wrong: it was never fundamentally about WiFi. WiFi made aborted attempts much more *frequent* (consistent with radio contention causing exactly that kind of transient mid-handshake hiccup), which is why "WiFi off" looked like a clean fix and "WiFi on" looked broken. The underlying bug fires with zero WiFi running.

What was observed, before the root cause was understood:

- **With WiFi compiled out** (`spike-ble-only`): BLE connected on the first attempt, every time — *until* an attempt aborted, after which nothing could connect until reboot. The early "reliable with WiFi off" conclusion came from a handful of attempts that all happened to succeed first try.
- **With the WiFi AP running** (`spike-ble`): most attempts never reached the TX (`bleConn` stayed 0), across both nRF Connect and LightBlue, two phones' Bluetooth stacks, a phone restart, and a board power-cycle. Only two connected across the whole session. Consistent with "contention causes an abort, the abort then bricks advertising, and every subsequent attempt fails for the second reason rather than the first."
- Once a connection *was* established, it was rock solid regardless of WiFi — see the 30-minute soak below.

**Ruled out:** `esp_coex_preference_set(ESP_COEX_PREFER_BT)` in `initBLE()` (`src/main.cpp`), biasing the shared radio's airtime toward BT. Call succeeds (`rc=0`) but changes nothing. Left in as harmless; it is not a fix. Note this was tried while the coexistence theory still looked right — it was aimed at the wrong target.

**Untested mitigation idea:** since the application gets no event, it cannot recover on demand — but it could recover blindly. A watchdog that soft-restarts the BLE stack (`NimBLEDevice::deinit()` then re-`init()` and re-advertise) on a timer whenever no client is connected would trade a brief advertising gap every few minutes for a bounded recovery window instead of needing a power cycle. Unattractive for a gig (an operator could still hit a dead window at the wrong moment) but it would prove whether a stack-level restart clears the stuck controller state at all, which is useful to know either way.

**Other avenues not yet tried:**

- Verbose logging to see what the controller actually does during the aborted attempt: `-DCONFIG_NIMBLE_CPP_LOG_LEVEL=4` and `-DCORE_DEBUG_LEVEL=5`. This should have been step one and would likely have found the root cause much sooner — do this first next session.
- Faster advertising interval (`setMinInterval`/`setMaxInterval`, ~20 ms). More advertising events means more chances for a connection request to land cleanly, so possibly fewer aborts in the first place — mitigation, not a fix.
- Check whether this is a known ESP-IDF / NimBLE-Arduino issue. Given how cleanly it reproduces, it may well be filed upstream already, possibly with a workaround or a fixed version. Worth searching before writing more code — this is a strong candidate for the real answer.
- Swap the host stack, to establish whether the bug is NimBLE-specific or sits in the shared controller layer below it. On the ESP32 there are really only two host stacks: **NimBLE** (current choice) and **Bluedroid**, the stock ESP-IDF stack exposed through Arduino's `BLEDevice` API. Bluedroid costs substantially more flash and RAM — the reason the design doc picked NimBLE in the first place — and `spike-ble` is already at 86.5% flash, so it would need `huge_app.csv` to fit. Worth doing as a *diagnostic* rather than a candidate implementation: if the same abort bricks advertising under Bluedroid too, the fault is in the shared controller/radio layer and switching libraries buys nothing. Note that "other BLE libraries" for Arduino-ESP32 (e.g. ESP32-BLE-Arduino) are wrappers over one of these two, not independent stacks — so they would not be an independent test.
- Try a newer or older NimBLE-Arduino release (currently 2.5.2) and/or a different espressif32 platform version (currently 7.1.3) — the controller blob ships with the platform package, so a platform bump changes the layer this bug most likely lives in.

### 2. Soak, 30 minutes — done, go (for a connection already established)

Run on the bench: `spike-ble` flashed, `soak.py --ble --minutes 30` from a Mac (no `--ws`; see below), TX serial captured in parallel.

- One reconnect at t=0 (scanner caught the TX before advertising had settled; reconnected in 7 s), then one unbroken connection for the remaining ~29.5 minutes.
- `ble_sent=3155 ble_state=3155 ble_drops=1 ble_errors=0` (client side); TX serial shows `bleCmd=3155 applied=3155` — every command the client sent was received and applied, zero loss end to end.
- `minheap` fell from 83,772 to 65,280 over the first ~9 minutes, then held *exactly* flat (identical to the byte) for the remaining ~21 minutes. A real leak would still be drifting down at minute 30; it wasn't. `maxblock` likewise flat at 59,380 from the same point.
- One boot line for the whole run (no resets), no panics, no watchdog triggers in the serial log.

Worth noting given the finding above: this soak's one connection happened to land on the first retry (7 s), and then never had to reconnect for 30 minutes. It doesn't contradict the connection-reliability problem — it just never needed to establish a second connection while WiFi was active.

Not yet run: `--ws` concurrently with `--ble`. `soak.py --ws` requires the test machine to join the TX's WiFi AP, which has no internet of its own — doing that from the machine running Claude Code would cut the session's own network connection for the duration, so this run used `--ble` only. To cover the WS+BLE combination, either run `soak.py --ws --ble` from a machine that isn't also driving this session, or run `--ws` from a second device at the same time as this `--ble` run.

### 3. RF check — not yet run

Blocked on the same thing: the BLE-on half of the comparison needs a BLE client connected and subscribed for the duration of the 80-second test, which is not currently reliable to set up. The BLE-off half (`spike-baseline`) could be run any time and is worth banking on its own, since it gives the baseline the comparison needs.

Record in #19: heap and flash numbers for both builds, the soak summary line, the `rf_score.py` table, and the verdict.

**Where the go/no-go actually stands:** flash and RAM fit, and a BLE link is solid once established — those parts pass. But the advertising bug above is a blocker in its own right, independent of everything the spike originally set out to measure: an operator who can't reliably connect mid-gig, and whose only recovery is power-cycling the TX, doesn't have a usable controller. It needs to be understood (and ideally fixed upstream or worked around) before a meaningful go call is possible. The useful next question isn't "do heap and RF pass" but "is this a known, fixable ESP32/NimBLE problem?"

Also worth revisiting in light of all this (raised but not decided): dropping WiFi from the TX entirely and making BLE the only transport, with Android served by Web Bluetooth from a hosted copy of the web UI — `iOS/DESIGN.md` section 6 sketches this. It would remove the coexistence dimension completely, at the cost of the zero-setup "just join the WiFi" fallback that works on any phone today. That is an #17 architecture decision, not a spike outcome.
