# BLE coexistence spike (#19)

One-day go / no-go for the iOS app (#17): can the TX run its WiFi AP, web server, LittleFS, RF24 **and** a BLE GATT service without falling over or hurting the radio link? Design background: `iOS/DESIGN.md` section 3.2 and section 7, risks 1 and 2.

Everything here is throwaway. If the answer is no-go, delete this folder, the two `spike-*` envs in `TX/platformio.ini` and the `#ifdef SPIKE` / `#ifdef BLE_SPIKE` blocks in `TX/src/main.cpp`. The normal build (`pio run`, env `az-delivery-devkit-v4`) has none of it compiled in.

No TX hardware to hand? [`harness/`](harness/README.md) runs the same WiFi + web + BLE load on a bare ESP32-S3 (or other dev board), with the radio simulated. It covers the soak and heap checks, not the RF check.

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

Record in #19: heap and flash numbers for both builds, the soak summary line, the `rf_score.py` table, and the verdict.
