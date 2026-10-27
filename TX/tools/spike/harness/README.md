# Coexistence test harness (#19)

A bare-board stand-in for the TX, so the BLE + WiFi part of the spike can run on any ESP32-S3 dev board, with no nRF24 or LEDs fitted. It runs the same load as `TX/src/main.cpp`:

- WiFi AP on channel 6 at `192.168.4.1`, including the TX's AMPDU workaround
- captive DNS, AsyncWebServer, WebSocket at `/ws` (same JSON protocol)
- LittleFS, serving a 64 KB `/blob` as a stand-in for the web UI assets
- the NimBLE GATT service with the same UUIDs as `iOS/DESIGN.md` 3.2
- one command queue drained on the loop task, as on the TX

The RF24 send is simulated with the same 5 x 10 ms blocking loop, and the board's RGB LED shows the mode colour (flashes white for strobe).

**What it can't tell you:** RF24 interference (risk 2) needs the real TX and receivers. The S3 also has a different radio and BLE controller from the TX's classic ESP32, so a pass here is encouraging but not the final go; repeat the soak on the TX board with `spike-ble` before phase 2.

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

Build sizes (espressif32 7.1.3, NimBLE-Arduino 2.5.2): `s3-ble` 976,929 bytes flash, 51,768 static RAM; `s3-noble` 811,413 and 45,404. BLE adds about 165 KB of flash and 6 KB of static RAM here.

## Serial output

```
Harness boot: reset=1 ble=on sketch=... freesketch=... heap=...
STATS {"uptime":10,"heap":...,"minheap":...,"maxblock":...,"ws":1,"wsCmd":...,"wsDisc":0,"http":...,"ble":1,"bleConn":1,"bleDisc":0,"bleCmd":...,"applied":...,"queueDrops":0,"reset":1}
```

A line every 10 s; type `t` for one now. `reset` other than 1 (power on) after a test means it crashed or a watchdog fired. The same JSON is at `http://192.168.4.1/stats`.

## Test runs

**1. Heap, BLE off vs on.** Flash `s3-noble`, join the AP from a phone, open `http://192.168.4.1/`, wait a minute, note `heap`, `minheap`, `maxblock`. Flash `s3-ble`, do the same with nRF Connect also connected and subscribed to State. The difference is what BLE costs at runtime.

**2. Soak, 30 minutes, everything at once.** With `s3-ble` and the monitor logging (`pio device monitor | tee soak.log`):

- Phone on the AP, test page open, tick both **Hammer** boxes. It sends a random mode every 300 ms (one in five a strobe) and downloads `/blob` back to back, and shows drops, reconnects and the command-to-state round trip.
- A BLE client hammering Command. Either a laptop: `python ../soak.py --ble --minutes 30` (add `--ws` if the laptop is also on the AP), or nRF Connect: write `01000000` (green), `62000000` (strobe) and so on to Command, with State notifications on.

Pass: no reboot (`reset` stays 1), `minheap` levels off instead of drifting down, `wsDisc` and `bleDisc` only move when you caused them, `queueDrops` at 0, the page's WS reconnects at 0, and no failed blobs.

**3. Feel.** While it's hammering, tap buttons by hand on the page and from nRF Connect; the LED and the "Mode now" line should keep up with no visible lag.

Record the numbers in #19 alongside the TX runs.
