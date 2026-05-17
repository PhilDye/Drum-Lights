# RF Heartbeat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the TX's 5×-burst-on-change with a 100 ms heartbeat that continuously broadcasts the current mode, so receivers self-heal from lost packets and late joiners catch up within one interval — while preserving zero-latency response to UI mode changes.

**Architecture:** TX maintains `CurrentMode` as today, but always sends one packet per heartbeat tick (10 Hz). UI mode changes call the same single-packet send immediately and reset the heartbeat clock. Mode 98 (strobe) becomes a single-packet transient that bypasses `CurrentMode` (so the heartbeat never broadcasts 98). RX becomes idempotent: it only mutates state when the received mode differs from the running one, so heartbeat duplicates are no-ops. The 5-flash strobe visual moves entirely to the RX (one packet → one 5-flash loop).

**Tech Stack:** PlatformIO; Arduino-ESP32 (TX, ESP32) + Arduino-ESP8266 (RX, ESP8266); RF24 library; FastLED (RX); ESPAsyncWebServer (TX); `millisDelay` for non-blocking timing.

**Reference spec:** [docs/superpowers/specs/2026-05-17-rf-heartbeat-design.md](../specs/2026-05-17-rf-heartbeat-design.md)

**Verification model:** This is bare-metal firmware with no automated test framework in the repo. Verification is build-time (`pio run`) plus a manual hardware test plan in Task 3. Do not invent or scaffold a test framework.

---

## Task 1: TX heartbeat, edge sends, and mode-98 transient

**Files:**
- Modify: `TX/src/main.cpp`
  - Constants block near line 45 (replace `RETRANSMITS` with `HEARTBEAT_INTERVAL`)
  - Auto-mode timing block near lines 51–58 (add `heartbeatDelay`)
  - Delete `broadcastRF()` (lines 143–151)
  - Add `sendModeRF()` helper in its place
  - Modify `handleWSMessage()` (lines 153–203): replace burst with edge send, fix `=`/`==` bug, restructure mode-98 handling
  - Modify `setup()` near line 279: start the heartbeat after `initRadio()`
  - Modify `loop()` (lines 298–317): add heartbeat tick, replace burst in AUTO block with edge send

Build with: `cd TX && pio run`

- [ ] **Step 1: Replace `RETRANSMITS` constant with `HEARTBEAT_INTERVAL`**

In `TX/src/main.cpp` at line 45, replace:
```cpp
const byte RETRANSMITS = 5; // how many times we retransmit every message, for reliability in noisy RF environments
```
with:
```cpp
const unsigned long HEARTBEAT_INTERVAL = 100; // ms — TX continuously rebroadcasts CurrentMode at this cadence so receivers self-heal from lost packets and late joiners catch up
```

- [ ] **Step 2: Add `heartbeatDelay` alongside `autoDelay`**

In `TX/src/main.cpp` immediately after line 53 (`millisDelay autoDelay; // the delay object`), add:
```cpp
millisDelay heartbeatDelay; // ticks every HEARTBEAT_INTERVAL ms to rebroadcast CurrentMode
```

- [ ] **Step 3: Replace `broadcastRF()` with `sendModeRF()`**

In `TX/src/main.cpp` at lines 143–151, replace the entire `broadcastRF()` function:
```cpp
void broadcastRF()
{
  for (size_t i = 0; i < RETRANSMITS; i++)
  {
    radio.write(&CurrentMode, sizeof(CurrentMode), true);
    delay(10);
  }
  Serial.printf("CurrentMode #%d broadcasted to RF\n", CurrentMode);
}
```
with:
```cpp
void sendModeRF(int mode)
{
  radio.write(&mode, sizeof(mode), true);
}
```

No serial print on every send — the heartbeat fires 10× per second and would flood the log. The mode-change paths log separately.

- [ ] **Step 4: Rewrite `handleWSMessage()`**

In `TX/src/main.cpp`, replace lines 153–203 (the entire `handleWSMessage` function) with:
```cpp
void handleWSMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    const size_t size = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<size> json;
    DeserializationError err = deserializeJson(json, data);
    if (err)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.c_str());
      return;
    }

    int newMode = json["mode"];
    Serial.printf("Received mode #%d\n", newMode);

    // Mode 98 (strobe) is a one-shot transient. Send a single packet directly
    // and do NOT update CurrentMode — the heartbeat must keep broadcasting
    // the actual steady-state pattern, not 98.
    if (newMode == 98)
    {
      sendModeRF(98);
      Serial.println("Strobe transient sent (CurrentMode unchanged)");
      return;
    }

    int previousMode = CurrentMode;

    if (newMode == AUTO_MODE)
    { // auto
      Serial.printf("AUTO mode set ON\n");
      newMode = autoModes[random(24)];
      Serial.printf("CurrentMode randomised to #%d\n", newMode);
      autoDelay.start(AUTO_TIME);
    }
    else if (previousMode == AUTO_MODE)
    {
      autoDelay.stop();
      Serial.printf("AUTO mode set OFF\n");
    }

    CurrentMode = newMode;
    Serial.printf("CurrentMode set to #%d\n", CurrentMode);

    // Edge send: zero-latency UI response. Restart the heartbeat clock so
    // the next heartbeat lands one full interval later, not piggy-backed.
    sendModeRF(CurrentMode);
    heartbeatDelay.restart();

    notifyClients();
  }
}
```

Key things this rewrite does:
- Short-circuits mode 98 at the top: single packet, no state change, return.
- Fixes the `else if (CurrentMode = AUTO_MODE)` assignment bug → `else if (previousMode == AUTO_MODE)`.
- Replaces `broadcastRF()` with `sendModeRF(CurrentMode); heartbeatDelay.restart();`.
- Removes the now-unnecessary `previousMode` save/revert dance around mode 98 (mode 98 returns before touching `CurrentMode`).

- [ ] **Step 5: Start the heartbeat in `setup()`**

In `TX/src/main.cpp`, the `setup()` function around line 279 currently calls `initRadio();`. Immediately after that line, add:
```cpp
  heartbeatDelay.start(HEARTBEAT_INTERVAL);
```

The line should now look like:
```cpp
  initRadio();
  heartbeatDelay.start(HEARTBEAT_INTERVAL);
```

- [ ] **Step 6: Add heartbeat tick and update AUTO block in `loop()`**

In `TX/src/main.cpp`, replace the entire `loop()` function (lines 298–317):
```cpp
void loop()
{
  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (autoDelay.justFinished())
  {
    // set a random mode
    CurrentMode = autoModes[random(24)];
    Serial.printf("CurrentMode randomised to #%d\n", CurrentMode);

    broadcastRF();
    notifyClients();

    autoDelay.repeat(); // repeat
    Serial.println("autoDelay restarted");
  }

  delay(DNS_INTERVAL);  // seems to help with stability, if you are doing other things in the loop this may not be needed
}
```
with:
```cpp
void loop()
{
  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (autoDelay.justFinished())
  {
    // set a random mode
    CurrentMode = autoModes[random(24)];
    Serial.printf("CurrentMode randomised to #%d\n", CurrentMode);

    sendModeRF(CurrentMode);
    heartbeatDelay.restart();
    notifyClients();

    autoDelay.repeat(); // repeat
    Serial.println("autoDelay restarted");
  }

  if (heartbeatDelay.justFinished())
  {
    sendModeRF(CurrentMode);
    heartbeatDelay.repeat();
  }

  delay(DNS_INTERVAL);  // seems to help with stability, if you are doing other things in the loop this may not be needed
}
```

- [ ] **Step 7: Build the TX firmware**

Run: `cd TX && pio run`

Expected: build completes with no errors. Warnings about the now-unused `previousMode` variable in earlier code paths should be gone (the rewrite removed it). No reference to `broadcastRF` or `RETRANSMITS` should remain in the file.

If the build fails, the most likely cause is a missed brace or a stale call site. Run `grep -n "broadcastRF\|RETRANSMITS" TX/src/main.cpp` — both greps should return nothing.

- [ ] **Step 8: Commit**

```bash
git add TX/src/main.cpp
git commit -m "feat(TX): replace 5x burst with 100ms heartbeat + zero-latency edge sends

TX now continuously rebroadcasts CurrentMode at 10 Hz so receivers
self-heal from lost packets and late joiners catch up within one
interval. UI mode changes still send immediately for zero perceptible
latency. Mode 98 (strobe) is a single-packet transient that bypasses
CurrentMode so the heartbeat never holds it. Also fixes a latent
assignment-vs-comparison bug in the AUTO-mode revert path."
```

---

## Task 2: RX idempotent receive and self-contained 5-flash strobe

**Files:**
- Modify: `RX/src/main.cpp`
  - `readRadio()` at lines 161–177 (add idempotency guard)
  - `case 98` at lines 336–343 (5-flash loop)

Build with: `cd RX && pio run`

- [ ] **Step 1: Make `readRadio()` idempotent**

In `RX/src/main.cpp`, replace the entire `readRadio()` function at lines 161–177:
```cpp
void readRadio()
{
  byte pipe;

  if (radio.available(&pipe))
  { // is there a payload?
    int payload;
    radio.read(&payload, sizeof(payload)); // get incoming payload
    ledMode = payload;

    Serial.print("Radio RX data: ");
    Serial.println(ledMode);

    // clear all pixels ready for the new mode
    FastLED.clear();
  }
}
```
with:
```cpp
void readRadio()
{
  byte pipe;

  if (radio.available(&pipe))
  { // is there a payload?
    int payload;
    radio.read(&payload, sizeof(payload)); // get incoming payload

    // Idempotent: heartbeats arrive ~10x per second carrying the current
    // mode. Only react when the mode actually changes — otherwise we'd
    // wipe running patterns at 10 Hz. This equality guard is also what
    // keeps mid-strobe heartbeats from re-triggering case 98 (which
    // restores ledMode to the prior mode after flashing).
    if (payload != ledMode)
    {
      ledMode = payload;
      Serial.print("Radio RX mode change: ");
      Serial.println(ledMode);
      FastLED.clear();
    }
  }
}
```

- [ ] **Step 2: Replace `case 98` with a self-contained 5-flash loop**

In `RX/src/main.cpp`, replace lines 336–343:
```cpp
  case 98:
    // quick white strobe - flashes multiple times because TX resends mode 3 times :-|
    fill_solid(leds, numLeds, CRGB::White);
    FastLED.show();
    FastLED.delay(30);
    FastLED.clear();
    ledMode = currentMode; // reinstate the previous mode
    break;
```
with:
```cpp
  case 98:
    // 5-flash white strobe — TX now sends mode 98 as a single packet,
    // so the RX produces all 5 flashes from one receive. Total ~300ms.
    // Heartbeats arriving during this loop are buffered by the nRF24's
    // 3-deep RX FIFO and are duplicates of the prior mode, so the
    // idempotency guard in readRadio() will no-op them after the strobe.
    for (int i = 0; i < 5; i++)
    {
      fill_solid(leds, numLeds, CRGB::White);
      FastLED.show();
      FastLED.delay(30);
      FastLED.clear();
      FastLED.show();
      FastLED.delay(30);
    }
    ledMode = currentMode; // restore prior mode (load-bearing — otherwise loop re-enters case 98)
    break;
```

- [ ] **Step 3: Build the RX firmware**

Run: `cd RX && pio run`

Expected: build completes with no errors.

- [ ] **Step 4: Commit**

```bash
git add RX/src/main.cpp
git commit -m "feat(RX): idempotent receive + self-contained 5-flash strobe

readRadio() now only mutates state when the received mode differs from
the running one, so the new TX heartbeat (10 Hz duplicates of the
current mode) does not disrupt running patterns. case 98 produces all
5 strobe flashes from a single received packet, since TX no longer
bursts."
```

---

## Task 3: Hardware verification

**Files:** none modified.

This task is the manual acceptance test plan from the design spec. It must be run on real hardware: one TX and at least two RX units (more is better for catch-up tests). Both firmwares from Tasks 1 & 2 must be flashed before starting.

- [ ] **Step 1: Flash both firmwares**

```bash
cd TX && pio run -t upload
cd ../RX && pio run -t upload   # repeat for each receiver
```

Open serial monitors on both: `pio device monitor -b 115200`.

- [ ] **Step 2: Verify heartbeat is firing**

With TX powered and no UI interaction:
- TX serial: silent (no per-heartbeat log — by design, to avoid spam).
- RX serial: shows the initial "Radio RX mode change: …" once shortly after the RX boots, then silence — confirming heartbeats are arriving but the idempotency guard is suppressing log output.

Quick sanity check that heartbeats are actually on the air: power-cycle the RX and observe it picks up the current mode within ≤100 ms of its radio init logging.

- [ ] **Step 3: Verify catch-up after RX reboot**

TX in any non-default mode (e.g., select Swan Samba twinkle in the UI). Power-cycle an RX. Expected: drum joins the running pattern within one heartbeat after radio init completes, with no user action needed.

Repeat 2–3 times to confirm consistency.

- [ ] **Step 4: Verify zero-latency UI mode changes**

Watch a drum while tapping different modes in the UI. Each tap should produce a visually immediate response — no perceptible lag. (The edge send fires on the WS event, well before the next heartbeat tick.)

- [ ] **Step 5: Verify no flicker on running patterns**

For each of these modes, run for ≥30 s and look for any 10 Hz visual disturbance:
- 63 (Swan Samba twinkle)
- 23 (Swan Samba chase)
- 50 (wood fire)
- 99 (rainbow)

Expected: patterns animate cleanly with no periodic glitch. If you see 10 Hz flicker, the idempotency guard in `readRadio()` is not working — re-check Task 2 Step 1.

- [ ] **Step 6: Verify strobe (mode 98)**

With any static mode running (e.g., mode 1 solid green):
- Tap the strobe button once.
- Expected: exactly 5 white flashes, then back to solid green with no visible glitch.

Repeat with twinkle running: 5 flashes, then twinkle resumes seamlessly.

- [ ] **Step 7: Verify AUTO mode**

Enable AUTO in the UI. Expected:
- A random mode is selected immediately (drum changes visibly within 100 ms).
- Every 30 s a new random mode is chosen and broadcast immediately.
- UI display tracks the current mode.

- [ ] **Step 8: Verify lossy-link recovery (optional but recommended)**

Walk an RX to the edge of radio range. Expected: patterns remain stable (heartbeats keep refreshing); mode changes still arrive within a few hundred ms even if the first edge packet is dropped.

- [ ] **Step 9: Document and ship**

If all checks pass, the implementation is complete. Suggested follow-ups (out of scope for this plan, but worth raising with the maintainer):
- Add a line to `README.md` flagging that pre-heartbeat receivers are incompatible with new-heartbeat TX, so all units must be flashed together.
- Consider a PR if changes were made on a branch.
