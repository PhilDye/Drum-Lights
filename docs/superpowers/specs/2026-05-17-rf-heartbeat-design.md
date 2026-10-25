# RF Heartbeat — Design

**Date:** 2026-05-17
**Status:** Approved (pending spec review)

## Problem

The current firmware transmits each mode change as a 5× burst over ~50 ms, then goes silent until the next user action or AUTO-mode timer. Two failure modes follow:

1. **Lost burst.** If all 5 packets of a burst are lost (noisy RF, momentary obstruction, RX briefly busy), the affected drum misses the mode change entirely until the next change.
2. **No catch-up.** A drum that reboots, loses power, or comes online late has no way to learn the current mode until the user changes mode again.

## Goal

A drum that misses any individual transmission, or that boots / reboots at any time, catches up to the current pattern within a fraction of a second — without measurable battery impact on the transmitter.

## Approach

The TX broadcasts the current mode on a fixed-interval heartbeat (100 ms). User-driven mode changes are sent immediately as a single packet *in addition to* the heartbeat — preserving zero-perceptible-latency UI response. The RX treats receives as idempotent: only reset state and clear the strip when the received mode differs from what it's currently running, so heartbeat duplicates do not disrupt running patterns.

This single mechanism solves both failure modes. Lost packets self-heal within one heartbeat interval. Late-joining receivers learn the current mode within one heartbeat interval.

## Design parameters

| Parameter | Value | Rationale |
|---|---|---|
| Heartbeat interval | 100 ms | Worst-case catch-up well under user-perceptible threshold; ≤0.6 % RF duty cycle. |
| Edge send on mode change | 1 packet, immediate | Zero-latency UI response; resets heartbeat clock. |
| Burst on mode change | None | Removed; single packet + heartbeat-self-heal supersedes it. |
| Payload | Unchanged (`int` mode value) | No protocol change needed. |
| Direction | Unchanged (TX-only) | No bidirectional / ack protocol. |

## TX changes ([TX/src/main.cpp](../../../TX/src/main.cpp))

### New state
- `const unsigned long HEARTBEAT_INTERVAL = 100; // ms`
- `millisDelay heartbeatDelay;`
- Started in `setup()` after `initRadio()`, repeated in `loop()`.

### New helper
```cpp
void sendModeRF(int mode)
{
  radio.write(&mode, sizeof(mode), true);
}
```
Single send. No `delay()`, no loop. Used by both the heartbeat tick and the edge sends.

### Heartbeat tick
In `loop()`:
```cpp
if (heartbeatDelay.justFinished())
{
  sendModeRF(CurrentMode);
  heartbeatDelay.repeat();
}
```

### Edge sends (zero-latency)
On any UI mode change in [handleWSMessage()](../../../TX/src/main.cpp):
- Update `CurrentMode` as today.
- `sendModeRF(CurrentMode); heartbeatDelay.restart();` so the next heartbeat tick lands one full interval later, not piggy-backed.

On AUTO-mode timer rollover in `loop()`:
- Pick new random mode, update `CurrentMode`.
- `sendModeRF(CurrentMode); heartbeatDelay.restart();`

### Mode 98 (strobe) — transient handling
`CurrentMode` must never hold `98`, otherwise the heartbeat would broadcast it indefinitely. In `handleWSMessage`, when `newMode == 98`:
- Call `sendModeRF(98)` directly (single packet, bypass the heartbeat).
- Do **not** update `CurrentMode`.
- Do **not** restart `heartbeatDelay` — the heartbeat continues broadcasting the actual prior mode unchanged.

This deletes the existing "save previousMode / revert after broadcast" dance — no longer needed.

### Cleanup
- Delete the `broadcastRF()` function.
- Delete the `RETRANSMITS` constant.
- Fix the assignment-vs-comparison bug at the existing `else if (CurrentMode = AUTO_MODE)` line. The naive fix `previousMode == AUTO_MODE` does not work because `CurrentMode` never actually holds `AUTO_MODE` while AUTO is running (the AUTO branch overwrites `newMode` with a real mode value before assigning it to `CurrentMode`). Use `autoDelay.isRunning()` as the source of truth for "AUTO is currently active". This also lets us drop the `previousMode` local — nothing else reads it once mode 98 short-circuits at the top.

## RX changes ([RX/src/main.cpp](../../../RX/src/main.cpp))

### Change 1 — idempotent receive
[readRadio()](../../../RX/src/main.cpp) only mutates state when the received mode differs from the current `ledMode`. Duplicate heartbeats are no-ops.

```cpp
void readRadio()
{
  byte pipe;
  if (radio.available(&pipe))
  {
    int payload;
    radio.read(&payload, sizeof(payload));
    if (payload != ledMode)
    {
      ledMode = payload;
      Serial.print("Radio RX mode change: ");
      Serial.println(ledMode);
      FastLED.clear();
    }
    // else: heartbeat duplicate; ignore. The equality guard is also what
    // protects mid-strobe heartbeats from re-triggering case 98.
  }
}
```

### Change 2 — case 98 self-contained 5-flash strobe
The TX now sends a single mode-98 packet (no burst). The RX must produce all 5 flashes from one receive:

```cpp
case 98:
  // 5-flash white strobe (timing matches old 5x-burst behaviour)
  for (int i = 0; i < 5; i++)
  {
    fill_solid(leds, numLeds, CRGB::White);
    FastLED.show();
    FastLED.delay(30);
    FastLED.clear();
    FastLED.show();
    FastLED.delay(30);
  }
  ledMode = currentMode; // restore prior mode; otherwise loop re-enters case 98
  break;
```

The existing `currentMode` capture at the top of `loop()` makes the revert correct — no change there.

### Mid-strobe safety
While the strobe loop runs (~300 ms), `readRadio()` is not being called. The nRF24L01's 3-deep hardware RX FIFO buffers incoming heartbeats. After the strobe completes:
- Buffered heartbeats are duplicates of the prior mode → equality check makes them no-ops.
- If the user changed to a *different* mode during the strobe (rare), the new-mode packet sits in FIFO or arrives on the next heartbeat, and the equality check picks it up — at most ~100 ms after the strobe ends.

The 5 flashes are unconditional once a mode-98 packet is received.

## Edge cases considered

| Scenario | Behaviour |
|---|---|
| RX boots / reboots mid-pattern | Catches up to current mode within ≤100 ms. |
| TX heartbeat packet lost | Next heartbeat ≤100 ms later self-heals. |
| Edge packet (mode change) lost | RX acts on next heartbeat ≤100 ms later. Strictly better than today (where a lost burst means a permanently missed change). |
| User picks same mode twice | Edge packet sent, but RX equality check makes it a no-op. No visible flicker. |
| User picks different mode during strobe | Strobe completes (5 flashes); new mode applied immediately after. |
| AUTO mode rollover | Random new mode sent immediately via edge send; heartbeat continues with that mode. |
| Mid-strobe heartbeats | Buffered in nRF24 FIFO, then equality-no-op'd after strobe. |
| Mode 98 in AUTO randomiser | N/A — AUTO mode array does not include 98. |

## Power impact

- ~10 single-packet transmits per second from heartbeat.
- nRF24L01 at 2 Mbps, 4-byte payload: ~600 µs on-air per packet → ~6 ms/s TX time = 0.6 % radio duty cycle.
- nRF24L01 TX current ~11 mA; average draw increase ≪ 100 µA.
- ESP32 baseline ~80 mA. Heartbeat is well below measurement noise on USB battery runtime.

## Verification (manual hardware test plan)

No automated test framework in the project; verification is on hardware.

1. **Heartbeat firing.** TX powered, no UI activity: serial log shows `sendModeRF` every ~100 ms; RX serial shows no "mode change" entries.
2. **Catch-up.** TX in mode 63. Power-cycle an RX. Drum joins the pattern within ≤100 ms of boot completion + radio init.
3. **Zero-latency edge.** Tap a new mode in the UI; drum responds with no perceptible delay.
4. **Lossy-link recovery.** At edge of radio range, patterns remain stable; mode changes arrive within a few hundred ms even if the initial edge packet is dropped.
5. **Strobe.** Tap mode 98 with a static pattern running: exactly 5 white flashes, then prior pattern resumes cleanly.
6. **No flicker on running patterns.** Run twinkle, chase, fire, rainbow ≥30 s each — no visible flicker from heartbeats.
7. **AUTO mode.** Enable AUTO; 30 s rotation continues; new random mode is broadcast immediately on rollover.

## Deployment note

TX and all RX units must be flashed together. A *new TX + old RX* combination is visibly broken — the old RX clears LEDs on every received packet, so heartbeats will wipe running patterns at 10 Hz. *New RX + old TX* works but loses the catch-up benefit. README should gain a one-line "upgrading from pre-heartbeat firmware" note.

## Explicitly out of scope

- Per-RX addressing or bidirectional protocol.
- Sequence numbers / packet IDs (payload equality is sufficient).
- New modes or effects.
- Refactoring the giant switch in RX `loop()`.
- Configurable heartbeat interval via ini file (compile-time `#define` is sufficient).
- TX web UI changes (none needed).
