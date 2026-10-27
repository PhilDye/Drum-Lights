# Coexistence test harness (#19)

A bare-board stand-in for the TX, so the BLE + WiFi part of the spike can run on any ESP32-S3 dev board, with no nRF24 or LEDs fitted. It runs the same load as `TX/src/main.cpp`:

- WiFi AP on channel 6 at `192.168.4.1`, including the TX's AMPDU workaround
- captive DNS, AsyncWebServer, WebSocket at `/ws` (same JSON protocol)
- LittleFS, serving a 64 KB `/blob` as a stand-in for the web UI assets
- the NimBLE GATT service with the same UUIDs as `iOS/DESIGN.md` 3.2, with every fix from the TX bench runs: `bleServer->start()`, `advertiseOnDisconnect(true)`, 20-30 ms advertising, and the 10 s stop+start re-arm watchdog
- one command queue drained on the loop task, as on the TX

The RF24 send is simulated with the same 5 x 10 ms blocking loop, and the board's RGB LED shows the mode colour (flashes white for strobe).

**What it's for:** the open question from the TX bench runs (`../README.md`, Results). On the classic ESP32, BLE connects first try only 70-80% of the time and takes 3-10 s, an aborted connect can kill advertising, and none of it is caused by WiFi. The S3 has a different BLE controller, so the harness answers "does a newer chip fix it?" under the full TX load.

**What it can't tell you:** RF24 interference (risk 2) needs the real TX and receivers.

## Build and flash

```sh
cd TX/tools/spike/harness
pio run -e s3-ble -t upload && pio device monitor
```

- `s3-ble`: with BLE (default). `s3-noble`: identical without BLE, for the heap comparison.
- Serial is on the **native USB** port (`ARDUINO_USB_CDC_ON_BOOT=1`). If your cable is in the **UART/COM** port, delete that flag in `platformio.ini`, or the monitor stays silent.
- The board profile assumes 8 MB flash. On a 4 MB board add `board_upload.flash_size = 4MB` and `board_build.partitions = default.csv` to `[env]`.
- RGB LED on GPIO 48. DevKitC-1 v1.1 boards use 38: add `-DLED_PIN=38`.
- AP `DrumLights-Harness`, password `drumlights`, BLE name `Drum Lights Harness`. Override with `-DAP_SSID=...`, `-DAP_PASS=...`, `-DBLE_NAME=...`.

Build sizes (espressif32 7.1.3, NimBLE-Arduino 2.5.2): `s3-ble` 977,701 bytes flash, 51,776 static RAM; `s3-noble` 811,413 and 45,404. BLE adds about 166 KB of flash and 6 KB of static RAM here, against 254 KB on the classic ESP32 TX.

## Serial output

```
Harness boot: reset=1 ble=on sketch=... freesketch=... heap=...
STATS {"uptime":10,"heap":...,"minheap":...,"maxblock":...,"ws":1,"wsCmd":...,"wsDisc":0,"http":...,"ble":1,"bleConn":1,"bleDisc":0,"bleCmd":...,"applied":...,"queueDrops":0,"reset":1}
```

A line every 10 s. `adv` is the host's `isAdvertising()` and `rearm` counts watchdog re-arms (`-1` when it's off).

Serial commands: `t` stats now, `w` toggles the re-arm watchdog (on at boot), `a` prints the advertising state and calls `start()`, `b` does `stop()` then `start()`. `reset` other than 1 (power on) after a test means it crashed or a watchdog fired. The same JSON is at `http://192.168.4.1/stats`.

## Test runs

**0. Connection reliability: the one that decides it.** From a laptop near the board, with the harness's WiFi AP running and a phone on the test page with both Hammer boxes ticked:

```sh
python ../ble_reliability.py --cycles 20
```

1. First with the watchdog **off** (`w` in the monitor, check it prints "off"). This shows whether the S3 has the aborted-connect bug: on the classic ESP32 one failed connect turned every later cycle into a scan failure.
2. Then with it **on** (`w` again), for a number to compare with the TX's 8/10 at 4.4 s median.

Run at least 20 cycles: the TX runs showed 10 can't tell 70% from 90%. If you see scan failures, type `a` in the monitor: `isAdvertising=1` while nothing can see the board is the same bug as on the ESP32. Then `b` should bring it back.

A sub-second median and 19-20/20 is a clear go for an S3 TX. 70-80% at multi-second times means the chip isn't the fix.

**1. Heap, BLE off vs on.** Flash `s3-noble`, join the AP from a phone, open `http://192.168.4.1/`, wait a minute, note `heap`, `minheap`, `maxblock`. Flash `s3-ble`, do the same with nRF Connect also connected and subscribed to State. The difference is what BLE costs at runtime.

**2. Soak, 30 minutes, everything at once.** With `s3-ble` and the monitor logging (`pio device monitor | tee soak.log`):

- Phone on the AP, test page open, tick both **Hammer** boxes. This covers the WebSocket side without the laptop joining the AP, so the laptop keeps its internet. It sends a random mode every 300 ms (one in five a strobe) and downloads `/blob` back to back, and shows drops, reconnects and the command-to-state round trip.
- A BLE client hammering Command. Either a laptop: `python ../soak.py --ble --minutes 30` (add `--ws` if the laptop is also on the AP), or nRF Connect: write `01000000` (green), `62000000` (strobe) and so on to Command, with State notifications on.

Pass: no reboot (`reset` stays 1), `minheap` levels off instead of drifting down, `wsDisc` and `bleDisc` only move when you caused them, `queueDrops` at 0, the page's WS reconnects at 0, and no failed blobs.

**3. Feel.** While it's hammering, tap buttons by hand on the page and from nRF Connect; the LED and the "Mode now" line should keep up with no visible lag.

Record the numbers in #19 alongside the TX runs.

## Results, measured 2026-10-27

Board: ESP32-S3, 16 MB flash, MAC `98:c3:77:a1:9d:94`, espressif32 7.1.3, NimBLE-Arduino 2.5.2. Client: macOS, the same Mac that produced the TX figures in `../README.md`. WiFi AP up on channel 6 throughout (`startAccessPoint()` runs before `startBLE()`, so BLE advertising is itself proof the AP came up).

### 0. Connection reliability — the S3 fixes it

| Build | Success | Median connect | Max |
| --- | --- | --- | --- |
| TX `spike-ble` (classic ESP32, for comparison) | 8/10 | 4.4 s | 10.2 s |
| S3 harness, re-arm watchdog **on** | **20/20** | **1.01 s** | 1.08 s |
| S3 harness, re-arm watchdog **off** | **20/20** | **1.03 s** | 1.15 s |

Forty cycles, every one connected, every connect between 0.96 and 1.15 s. All 40 disconnects were `reason 531` (`BLE_ERR_REM_USER_CONN_TERM`, the normal client-initiated close) — **no supervision timeouts**, where the classic ESP32 produced 2 spontaneous link losses in 8 connections on the iPhone run.

The watchdog-off result is the decisive one. On the classic ESP32 the stop+start re-arm is the difference between unusable and mostly working: without it, one aborted connect kills advertising permanently and every later cycle fails at the *scan* stage. With it disabled on the S3, nothing degraded — 20/20, `rearm: -1` in the stats confirming it was off. **The S3 does not exhibit the aborted-connect advertising bug.** The watchdog is kept because it is harmless and the TX still needs it.

The ~1.0 s floor is suspiciously consistent across all 40 cycles and is most likely CoreBluetooth's own connection-setup cost, not something the firmware controls. Don't expect to tune it lower.

Caveat: measured from macOS only. On the TX, Mac and iPhone agreed closely (8/10 at 4.4 s vs 8/10 at 4-10 s), so this probably carries over, but an iPhone run on an S3 is the one check still outstanding before committing to the chip.

### Heap: far more headroom than the classic ESP32

| | TX `spike-ble` | S3 harness |
| --- | --- | --- |
| Free heap after setup | 85,368 | **150,176** |
| `minheap`, idle | ~83,500 | 148,660 |
| `maxblock` | 65,524 | 139,252 |

The ~40 KB `minheap` / ~20 KB `maxblock` bar this spike set is comfortably cleared, with the worst transient over the whole soak at 110,672 / 118,772.

### 2. Soak — found a crash, and it is not BLE

15 minutes (shortened from 30 once the failure was understood), `s3-ble`, BLE commands every 500 ms from `../soak.py --ble`, **plus a phone on the test page with both Hammer boxes ticked** — so this is the simultaneous WebSocket + BLE load the TX runs never achieved.

**It panicked after about 4 minutes:**

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

Mechanism: every mode change calls `notifyAll()` → `ws.textAll()`. At roughly 5 broadcasts/second to two WebSocket clients, `AsyncWebSocket`'s per-client queue overflowed (the 213 errors, each a discarded message). A client then dropped, lwIP delivered a TCP error, and `AsyncClient::_error()` called `tcp_sent()` on a pcb lwIP had already freed when it raised that error — which lwIP's API forbids. Assert, panic, reboot.

**This is a bug in `AsyncTCP-esphome`, not in BLE, not in coexistence, and not S3-specific.** The production TX runs the same `AsyncTCP-esphome` / `ESPAsyncWebServer-esphome` and the same broadcast-on-every-change pattern, so **the TX has this latent crash today, with or without BLE.** The TX's own soak never hit it because it ran BLE-only with no WebSocket client attached. Real TX use is gentler than this test (~5 broadcasts/s to 2 clients is well above an operator tapping buttons), but strobe-heavy use with the web UI open is the same shape of load, and the failure reboots the transmitter mid-set.

Everything else in the soak was clean, including the recovery:

| | |
| --- | --- |
| Panics / reboots | 1 (at ~4 min); recovered in ~4 s unaided |
| BLE commands applied | 1,214 after the reboot, 437 before — 1,651 for 1,650 sent, i.e. none lost |
| Modes applied | 3,032 | 
| `queueDrops` (our own mode queue) | 0 |
| BLE disconnects | 0 (the link only dropped via the reboot) |
| WS client disconnects | 4 (queue-overflow casualties) |
| Free heap at 11 min | 144,984 — flat from 145,180 at 10 s |

Free heap being flat over 11 minutes is the no-leak evidence. `minheap` stepped down over the run (150,020 → 110,672) but that is a low-water mark and can only ever decrease, so its steps record transient peaks — here the WebSocket reconnect bursts — rather than a leak.

### Not covered

- **RF24 interference (risk 2):** needs the real TX and receivers; the harness simulates the radio.
- **iPhone connect reliability on an S3:** see the caveat above.
- **A clean WS+BLE soak:** blocked by the AsyncTCP crash. Worth rerunning once `notifyAll()` is rate-limited, to confirm nothing else hides behind it.
