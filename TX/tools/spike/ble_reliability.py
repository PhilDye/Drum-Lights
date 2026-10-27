#!/usr/bin/env python3
"""Measure BLE connection-establishment reliability against the TX (#19).

Repeats connect -> subscribe to State -> hold -> disconnect N times, scanning
fresh each cycle (so it's exercising the same path a phone's BLE stack does),
and reports a success rate and connect-latency distribution. Point this at
spike-ble-only (WiFi fully compiled out) to establish BLE's own ceiling, then
at spike-ble (WiFi running) to quantify how much worse coexistence makes it.

Usage: ble_reliability.py [--cycles N] [--hold SECONDS]
"""
import argparse
import asyncio
import statistics
import time

from bleak import BleakClient, BleakScanner

SERVICE_UUID = "9a9476f2-ae0d-406a-847a-d0fecaf34ce5"
STATE_UUID = "e1ebaf93-75c1-4681-8f5b-d07d93d86fde"


async def one_cycle(hold):
    t0 = time.monotonic()
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: SERVICE_UUID in [u.lower() for u in ad.service_uuids], timeout=15
    )
    if device is None:
        return {"ok": False, "stage": "scan", "elapsed": time.monotonic() - t0}

    t1 = time.monotonic()
    try:
        async with BleakClient(device, timeout=15) as client:
            t2 = time.monotonic()
            await client.start_notify(STATE_UUID, lambda _, data: None)
            await asyncio.sleep(hold)
            return {"ok": True, "scan_s": t1 - t0, "connect_s": t2 - t1, "elapsed": time.monotonic() - t0}
    except Exception as e:
        return {"ok": False, "stage": f"connect: {e!r}", "elapsed": time.monotonic() - t0}


async def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--cycles", type=int, default=20)
    p.add_argument("--hold", type=float, default=2.0, help="seconds to hold each connection")
    args = p.parse_args()

    results = []
    for i in range(args.cycles):
        r = await one_cycle(args.hold)
        results.append(r)
        tag = "OK" if r["ok"] else f"FAIL ({r['stage']})"
        detail = f"scan={r.get('scan_s', 0):.2f}s connect={r.get('connect_s', 0):.2f}s" if r["ok"] else ""
        print(f"[{i + 1}/{args.cycles}] {tag} {detail} (total {r['elapsed']:.1f}s)", flush=True)
        await asyncio.sleep(1)  # let the TX's advertising settle between cycles

    oks = [r for r in results if r["ok"]]
    print()
    print(f"Success: {len(oks)}/{len(results)} ({100 * len(oks) / len(results):.0f}%)")
    if oks:
        connect_times = [r["connect_s"] for r in oks]
        print(
            f"Connect time: min={min(connect_times):.2f}s "
            f"median={statistics.median(connect_times):.2f}s "
            f"max={max(connect_times):.2f}s"
        )
    fails = [r for r in results if not r["ok"]]
    if fails:
        print(f"Failure stages: {[f['stage'] for f in fails]}")


if __name__ == "__main__":
    asyncio.run(main())
