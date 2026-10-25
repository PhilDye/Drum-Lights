#!/usr/bin/env python3
"""Soak test for the BLE spike (#19): hammer the TX over WebSocket and/or BLE.

Run from a laptop joined to the TX WiFi AP (for --ws) and with Bluetooth on
(for --ble). Both can run at once, which is the point of the test. Prints a
summary line every 30 s and at the end; reconnects and counts every drop.

  pip install websockets bleak
  python soak.py --ws --ble --minutes 30

A phone running nRF Connect can stand in for --ble (write int32 LE values to
the Command characteristic), but this script counts the failures for you.
"""
import argparse
import asyncio
import json
import random
import struct
import time

WS_URL = "ws://192.168.4.1/ws"
SERVICE_UUID = "9a9476f2-ae0d-406a-847a-d0fecaf34ce5"
STATE_UUID = "e1ebaf93-75c1-4681-8f5b-d07d93d86fde"
COMMAND_UUID = "53f118a7-863e-429a-bf92-ac19864c8219"
INFO_UUID = "b1ab44f5-a9a1-4152-852b-50a7d7ec8427"

# the auto modes plus strobe; avoids -1 so auto doesn't fight the test
MODES = [1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 81, 82, 83, 84, 85, 86, 87, 88, 98]

stats = {
    "ws_sent": 0, "ws_state": 0, "ws_drops": 0,
    "ble_sent": 0, "ble_state": 0, "ble_drops": 0, "ble_errors": 0,
}


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


async def ws_client(interval, deadline):
    import websockets

    while time.monotonic() < deadline:
        try:
            async with websockets.connect(WS_URL, open_timeout=10) as ws:
                log(f"WS connected, first state: {await asyncio.wait_for(ws.recv(), 5)}")

                async def reader():
                    async for msg in ws:
                        json.loads(msg)
                        stats["ws_state"] += 1

                read_task = asyncio.create_task(reader())
                try:
                    while time.monotonic() < deadline:
                        if read_task.done():
                            read_task.result()  # raises the reason the socket closed
                        await ws.send(json.dumps({"mode": random.choice(MODES)}))
                        stats["ws_sent"] += 1
                        await asyncio.sleep(interval)
                finally:
                    read_task.cancel()
                return
        except Exception as e:
            stats["ws_drops"] += 1
            log(f"WS dropped: {e!r}; reconnecting")
            await asyncio.sleep(2)


async def ble_client(interval, deadline):
    from bleak import BleakClient, BleakScanner

    while time.monotonic() < deadline:
        try:
            device = await BleakScanner.find_device_by_filter(
                lambda d, ad: SERVICE_UUID in [u.lower() for u in ad.service_uuids], timeout=15
            )
            if device is None:
                log("BLE: TX not found; rescanning")
                continue
            async with BleakClient(device) as client:
                info = await client.read_gatt_char(INFO_UUID)
                log(f"BLE connected to {device.name} ({device.address}), info: {info.decode()}")

                def on_state(_, data):
                    struct.unpack("<iB", data)  # int32 mode + uint8 flags
                    stats["ble_state"] += 1

                await client.start_notify(STATE_UUID, on_state)
                while time.monotonic() < deadline:
                    if not client.is_connected:
                        raise ConnectionError("disconnected")
                    try:
                        await client.write_gatt_char(COMMAND_UUID, struct.pack("<i", random.choice(MODES)), response=True)
                        stats["ble_sent"] += 1
                    except Exception as e:
                        stats["ble_errors"] += 1
                        log(f"BLE write failed: {e!r}")
                    await asyncio.sleep(interval)
                return
        except Exception as e:
            stats["ble_drops"] += 1
            log(f"BLE dropped: {e!r}; reconnecting")
            await asyncio.sleep(2)


async def reporter(deadline):
    while time.monotonic() < deadline:
        await asyncio.sleep(30)
        log(" ".join(f"{k}={v}" for k, v in stats.items()))


async def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ws", action="store_true", help="hammer the WebSocket")
    p.add_argument("--ble", action="store_true", help="hammer the BLE Command characteristic")
    p.add_argument("--minutes", type=float, default=30)
    p.add_argument("--interval", type=float, default=0.5, help="seconds between commands per client")
    args = p.parse_args()
    if not (args.ws or args.ble):
        p.error("pick --ws, --ble or both")

    deadline = time.monotonic() + args.minutes * 60
    tasks = [reporter(deadline)]
    if args.ws:
        tasks.append(ws_client(args.interval, deadline))
    if args.ble:
        tasks.append(ble_client(args.interval, deadline))
    await asyncio.gather(*tasks)
    log("DONE " + " ".join(f"{k}={v}" for k, v in stats.items()))


if __name__ == "__main__":
    asyncio.run(main())
