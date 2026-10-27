"""A stand-in for the TX's WebSocket, for testing WebSocketTransport without hardware.

    pip install websockets
    python3 Tools/fake_tx.py &
    DRUMLIGHTS_WS_URL=ws://127.0.0.1:8765/ws swift test --filter WebSocketTransportTests

Follows the firmware: state on connect, and every change echoed to every client.
"""
import asyncio
import json
import random

import websockets

AUTO, STROBE = -1, 98
AUTO_MODES = [1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 81, 82, 83, 84, 85, 86, 87, 88]

state = {"mode": 13, "auto": False}
clients = set()


def message():
    return json.dumps({**state, "proto": 1, "fw": "fake", "catalogue": 1})


async def handler(ws):
    clients.add(ws)
    await ws.send(message())
    try:
        async for text in ws:
            mode = json.loads(text)["mode"]
            if mode == AUTO:
                state.update(mode=random.choice(AUTO_MODES), auto=True)
            elif mode != STROBE:  # strobe is one-shot: mode and auto are unchanged
                state.update(mode=mode, auto=False)
            websockets.broadcast(clients, message())
    finally:
        clients.discard(ws)


async def main():
    async with websockets.serve(handler, "127.0.0.1", 8765):
        await asyncio.Future()


asyncio.run(main())
