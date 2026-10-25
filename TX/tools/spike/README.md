# BLE coexistence spike (#19)

One-day go / no-go for the iOS app (#17): can the TX run its WiFi AP, web server, LittleFS, RF24 **and** a BLE GATT service without falling over or hurting the radio link? Design background: `iOS/DESIGN.md` section 3.2 and section 7, risks 1 and 2.

Everything here is throwaway. If the answer is no-go, delete this folder, the two `spike-*` envs in `TX/platformio.ini` and the `#ifdef SPIKE` / `#ifdef BLE_SPIKE` blocks in `TX/src/main.cpp`. The normal build (`pio run`, env `az-delivery-devkit-v4`) has none of it compiled in.

## Builds

| Env | What it is |
| --- | --- |
| `spike-baseline` | Normal firmware + heap telemetry + RF test. BLE not compiled in. |
| `spike-ble` | The same + NimBLE-Arduino with the GATT service below. |

Flash with `pio run -e spike-ble -t upload` (and `-t uploadfs` once for the web UI, as usual). CI builds all three envs and lists their RAM and flash use in the job summary.

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

### 2. Soak, 30 minutes — done, go

Run on the bench: `spike-ble` flashed, `soak.py --ble --minutes 30` from a Mac (no `--ws`; see below), TX serial captured in parallel.

- One reconnect at t=0 (scanner caught the TX before advertising had settled; reconnected in 7 s), then one unbroken connection for the remaining ~29.5 minutes.
- `ble_sent=3155 ble_state=3155 ble_drops=1 ble_errors=0` (client side); TX serial shows `bleCmd=3155 applied=3155` — every command the client sent was received and applied, zero loss end to end.
- `minheap` fell from 83,772 to 65,280 over the first ~9 minutes, then held *exactly* flat (identical to the byte) for the remaining ~21 minutes. A real leak would still be drifting down at minute 30; it wasn't. `maxblock` likewise flat at 59,380 from the same point.
- One boot line for the whole run (no resets), no panics, no watchdog triggers in the serial log.

Not yet run: `--ws` concurrently with `--ble`. `soak.py --ws` requires the test machine to join the TX's WiFi AP, which has no internet of its own — doing that from the machine running Claude Code would cut the session's own network connection for the duration, so this run used `--ble` only. To cover the WS+BLE combination, either run `soak.py --ws --ble` from a machine that isn't also driving this session, or run `--ws` from a second device at the same time as this `--ble` run.

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

Not obtained: a true simultaneous WS + BLE reading. Every attempt to hold both at once ran into the BLE connection reliability problem below before a steady state was reached. The WS-only `minheap`/`maxblock` numbers above vary run to run and one sample dipped to 24,212/18,420 — below the ~40 KB/~20 KB rough bar this doc set earlier — worth another look once BLE connects reliably enough to add to that figure.

### BLE connection reliability under WiFi — the actual finding

This dominated the session, so it's worth recording in detail. Across many connection attempts (nRF Connect and LightBlue, two phones' worth of Bluetooth stacks, one phone restart, one full board power-cycle):

- **With the WiFi AP + web server fully compiled out** (ad hoc `spike-ble-only` env, `-DSPIKE_NO_WIFI`, added purely to isolate this): BLE connected on the first attempt, every time it was tried.
- **With the WiFi AP running** (`spike-ble`, the real build): BLE connection attempts mostly never reached the TX at all — `bleConn` stayed 0 on the device's own telemetry, meaning the attempt failed before the link layer ever completed, regardless of client app, phone, or whether a WS client was also connected. Out of many attempts across the session, only two connected successfully.
- Once a BLE connection *did* establish with WiFi running, it was solid — see the 30-minute soak below, which ran the entire time with WiFi active and had zero issues after the initial connect.

So the problem is specifically **establishing** a BLE connection while the WiFi AP is active, not holding one. This is risk 1 from `iOS/DESIGN.md` section 7, and the evidence points toward a real coexistence issue rather than a one-off firmware bug — nothing in the two fixes above explains an intermittent failure that disappears when WiFi is compiled out entirely. Not root-caused further this session (ran out of time); candidates for next time: WiFi AP beacon/DNS/DHCP timing starving the BLE controller's connection-setup window, or ESP-IDF's software coexistence arbitration needing explicit tuning rather than its defaults.

`spike-ble-only` is ad hoc, added only for this diagnosis — delete it along with the rest of the spike if going no-go, don't carry it into phase 2.

### 2. Soak, 30 minutes — done, go (for a connection already established)

Run on the bench: `spike-ble` flashed, `soak.py --ble --minutes 30` from a Mac (no `--ws`; see below), TX serial captured in parallel.

- One reconnect at t=0 (scanner caught the TX before advertising had settled; reconnected in 7 s), then one unbroken connection for the remaining ~29.5 minutes.
- `ble_sent=3155 ble_state=3155 ble_drops=1 ble_errors=0` (client side); TX serial shows `bleCmd=3155 applied=3155` — every command the client sent was received and applied, zero loss end to end.
- `minheap` fell from 83,772 to 65,280 over the first ~9 minutes, then held *exactly* flat (identical to the byte) for the remaining ~21 minutes. A real leak would still be drifting down at minute 30; it wasn't. `maxblock` likewise flat at 59,380 from the same point.
- One boot line for the whole run (no resets), no panics, no watchdog triggers in the serial log.

Worth noting given the finding above: this soak's one connection happened to land on the first retry (7 s), and then never had to reconnect for 30 minutes. It doesn't contradict the connection-reliability problem — it just never needed to establish a second connection while WiFi was active.

Not yet run: `--ws` concurrently with `--ble`. `soak.py --ws` requires the test machine to join the TX's WiFi AP, which has no internet of its own — doing that from the machine running Claude Code would cut the session's own network connection for the duration, so this run used `--ble` only. To cover the WS+BLE combination, either run `soak.py --ws --ble` from a machine that isn't also driving this session, or run `--ws` from a second device at the same time as this `--ble` run.

### 3. RF check — not yet run

Record in #19: heap and flash numbers for both builds, the soak summary line, the `rf_score.py` table, and the verdict. Given the connection-reliability finding above, the go/no-go call likely hinges more on that than on heap or RF numbers — a BLE control surface an operator can't reliably connect to mid-gig is a problem independent of how clean it runs once connected.
