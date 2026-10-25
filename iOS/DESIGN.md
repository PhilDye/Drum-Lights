# Drum Lights iOS app: design proposal

Status: proposal, nothing built yet. This document covers what a native iOS controller should do, how it talks to the existing transmitter (TX), what has to change in the TX firmware, and what the web UI keeps doing for everyone without the app.

## 1. Should this exist at all?

Be honest about the audience first. Only one or two people in a band control the lights at any time. Everyone else just wears a receiver. So the app serves a handful of operators, and it competes with a web UI that already works for any phone.

A native app is only worth building if it does things the web page cannot. On iOS, Safari and the captive-portal browser can't:

| Capability | Web page on TX | Native app |
| --- | --- | --- |
| Talk to the TX without joining its WiFi | No | Yes, over Bluetooth LE |
| Keep the phone's mobile data / normal WiFi while controlling | No (phone is on a dead-end hotspot) | Yes, with BLE |
| Haptic feedback on a tap | No (no Vibration API in iOS Safari) | Yes |
| Control from the lock screen / Dynamic Island | No | Yes, Live Activity |
| Action Button, Control Centre, Shortcuts | No | Yes, App Intents |
| Apple Watch control | No | Yes |
| Stay connected when the screen locks | No (sockets suspended) | Yes, BLE background mode |
| Saved palettes that survive the captive-portal sheet closing | Fragile | Yes |

That list is long enough to justify the app, with one caveat: **almost all of the value depends on Bluetooth LE.** An app that only reuses the WiFi WebSocket inherits the worst parts of the web experience (joining a hotspot with no internet, iOS showing the captive sheet, sockets dying when the screen locks) and adds only haptics and nicer layout. If the BLE spike in section 7 fails, the right move is to improve the web UI and not build the app.

Costs to accept up front:

- Apple Developer Program fee (yearly) to put the app on real devices for more than 7 days.
- Distribution: TestFlight builds expire after 90 days, which is a real risk of turning up to a gig with a dead app. Use App Store distribution (public or Apple's "unlisted" option) so installs don't expire.
- Android operators get nothing from this. The web UI must stay a first-class controller (section 6).

## 2. Goals and non-goals

Goals:

1. One-hand, eyes-mostly-up control during a performance, with confirmation by touch (haptics) rather than by looking.
2. No WiFi switching. Phone keeps internet; the app talks to the TX directly.
3. Let the operator build shortlists (palettes) of the patterns they actually use, then jump straight to any of them with one tap, from the phone, the lock screen, a keyboard/pedal or the watch. Performances follow the music, not a script, so this is a palette to pick from, not a playlist to step through.
4. Stay in sync with any other controllers (web or app) at the same time.
5. Change nothing on the receivers. Every RX on every drum keeps its current firmware.

Non-goals (for now):

- Per-drum or per-section addressing. Needs RX firmware and new RF addressing; the README already identifies the UI as the hard part, and the app could be that UI later.
- Voice control. Siri in the middle of a samba band is not going to hear anything. App Intents give Siri support for free anyway; don't design for it.
- Beat detection from the microphone. Interesting, but a separate project (see section 10).

## 3. How the app talks to the TX

### 3.1 Transport choice

Primary transport: **Bluetooth LE GATT service on the TX ESP32.** Fallback: **the existing WebSocket at `ws://192.168.4.1/ws`.**

Why BLE:

- The ESP32 dev-kit already has BLE. No new hardware.
- Phone stays on its normal network. No hotspot join, no captive-portal sheet, no "this network has no internet" nags.
- iOS keeps a BLE connection alive in the background with the `bluetooth-central` background mode. That is what makes lock-screen buttons, Control Centre, the Action Button and the watch work. A WebSocket gets suspended within seconds of the screen locking.
- Range of 10 to 30 m outdoors is plenty: the TX sits at the back of the band and the operator is usually within it.

Why keep the WebSocket in the app:

- It works with today's firmware, so the app is usable before the BLE firmware ships, and with other people's builds that never update.
- Costs little: both transports carry the same commands (section 3.3).

The app joins the TX hotspot itself with `NEHotspotConfiguration` (SSID and password entered once in Settings) when it falls back to WiFi, so the user never visits the Settings app.

### 3.2 BLE GATT service (new, TX firmware)

Use [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) rather than the stock Bluedroid stack: it uses far less RAM and flash, which matters next to WiFi + AsyncWebServer + LittleFS on the same chip.

Advertised name: `Drum Lights TX` (configurable in `secrets.h` so neighbouring bands don't collide).

Service `9A9476F2-AE0D-406A-847A-D0FECAF34CE5`:

| Characteristic | UUID | Properties | Value |
| --- | --- | --- | --- |
| State | `E1EBAF93-75C1-4681-8F5B-D07D93D86FDE` | read, notify | `int32 mode` (LE) + `uint8 flags` (bit 0 = auto running) |
| Command | `53F118A7-863E-429A-BF92-AC19864C8219` | write (encrypted) | `int32 mode` (LE), same numbers as the WebSocket, `-1` = auto |
| Info | `B1AB44F5-A9A1-4152-852B-50A7D7EC8427` | read | small UTF-8 JSON, e.g. `{"proto":1,"fw":"1.4.0","catalogue":3}` |

Notes:

- Use write-with-response for Command. The response round trip is one connection interval (15 to 30 ms on iOS), and it tells the app the TX accepted the command, which drives the haptic.
- Security: require an encrypted, bonded link with a **static 6-digit passkey** from `secrets.h`. iOS asks for the PIN once and then remembers the TX. Without this, anyone in the crowd with a BLE scanner app could change the lights. NimBLE stores a small number of bonds by default (`CONFIG_BT_NIMBLE_MAX_BONDS`); raise it to cover the number of operator phones and watches.
- Allow at least 3 simultaneous connections (two phones and a watch).

### 3.3 One command path in the firmware

Today `handleWSMessage()` parses JSON and contains all the mode logic. Split it so both transports share one function:

```cpp
void applyMode(int newMode);   // auto handling, strobe revert, broadcastRF(), notifyAll()
void notifyAll();              // ws.textAll(...) + BLE State notify
```

`handleWSMessage()` becomes "parse JSON, call `applyMode`". The BLE Command write callback becomes "read int32, call `applyMode`". Every change, from any source (web, app, watch, the auto timer), goes out to every connected client, so multiple controllers stay in sync.

### 3.4 WebSocket protocol v1 (backward compatible)

Client to TX is unchanged: `{"mode": n}`.

TX to client gains fields that the current web UI simply ignores:

```json
{"mode": 13, "auto": false, "proto": 1, "fw": "1.4.0", "catalogue": 3}
```

and the TX sends it **immediately on `WS_EVT_CONNECT`**, so a new client shows the real state instead of a blank screen.

### 3.5 Existing firmware issues this design depends on fixing

Found while reading `TX/src/main.cpp`; they affect the web UI today and the app would inherit them:

1. **New clients don't get the current state.** `notifyClients()` only runs after a change. Fix: send state on connect (above).
2. **Auto mode is invisible to clients.** In auto, `CurrentMode` holds the random mode, so no UI can show that auto is running and the Auto button never lights up. Fix: keep an explicit `autoRunning` flag and include it in state.
3. **`else if (CurrentMode = AUTO_MODE)`** is an assignment, not a comparison. It happens to work (auto stops on any manual command) but only by accident, and it means a **Strobe during Auto stops Auto**, which is probably not wanted. Decide the intended behaviour, then write it explicitly.

### 3.6 Room to grow the RF payload without touching receivers

The RF24 link uses fixed 32-byte payloads with dynamic payloads off. The TX writes a 4-byte `int` and every RX reads only the first 4 bytes. So the TX can append extra bytes (speed, brightness, a drum-group mask) **and old receivers will ignore them**. New receivers can opt in later. Nothing in this proposal needs that, but it means the app's protocol should carry an optional parameter block from day one rather than baking "a command is one int" into everything.

## 4. One mode catalogue for every UI

Today the mode numbers live in four places: RX `switch`, TX `autoModes[]`, the web UI's hand-written buttons, and `RX/src/prototypes.h` comments. Adding an app would make five. Instead, add `TX/app/src/modes.json` as the single source for the UIs:

```json
{
  "catalogue": 3,
  "colours": {
    "green":  {"name": "Green",  "rgb": "#00C853"},
    "yellow": {"name": "Yellow", "rgb": "#FFD600"},
    "blue":   {"name": "Blue",   "rgb": "#2962FF"}
  },
  "effects": [
    {"id": "solid",   "name": "Solid",   "symbol": "circle.fill",
     "modes": {"green": 1, "yellow": 2, "blue": 3, "red": 4, "white": 5, "cyan": 6, "pink": 7, "orange": 8}},
    {"id": "chase",   "name": "Chase",   "symbol": "arrow.trianglehead.clockwise",
     "modes": {"green": 11, "yellow": 12, "green+red": 21, "yellow+blue": 22}},
    {"id": "special", "name": "Specials",
     "items": [
       {"name": "Rio Spin", "mode": 91},
       {"name": "999", "mode": 199, "confirm": true}
     ]}
  ],
  "actions": {"off": 0, "auto": -1, "strobe": 98},
  "auto": [1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 81, 82, 83, 84, 85, 86, 87, 88]
}
```

- The web UI (Vite) imports it and renders buttons from it instead of hard-coded HTML. Vite copies it into `data/` so the TX serves it too.
- The app bundles a copy and compares `catalogue` against the TX Info characteristic. On mismatch it shows "Transmitter has newer effects; update the app" and still works for known modes. (Fetching the catalogue over BLE is possible but needs chunking past 512 bytes; not worth it while effects change a few times a year.)
- A small build step can generate the TX `autoModes[]` from it, or leave that hand-maintained and add a check.

## 5. The app

### 5.1 Platform and structure

- SwiftUI, iOS 17 minimum (interactive Live Activities). Control Centre controls light up on iOS 18 via `#available`.
- Targets:
  - `DrumLightsKit` (Swift package): catalogue model, protocol encoding, `Transport` protocol with `BLETransport` (CoreBluetooth) and `WebSocketTransport` (`URLSessionWebSocketTask`), plus `MockTransport` for SwiftUI previews and the Simulator (which has no Bluetooth). Shared by everything below.
  - iOS app.
  - Widget extension: Live Activity and Control Centre controls.
  - watchOS app.
- State: a single `@Observable` `LightsController` owns the connection, current state and pending command. Palettes stored with SwiftData.
- `Info.plist`: `NSBluetoothAlwaysUsageDescription`, `UIBackgroundModes = bluetooth-central`, CoreBluetooth state restoration identifier; `NSLocalNetworkUsageDescription` and the Hotspot Configuration entitlement for the WiFi fallback.

### 5.2 Screens

Two tabs, standard `TabView`:

**Live** (default)

```
┌──────────────────────────────┐
│ ● Connected · BLE        ⚙︎   │  status pill, not a blocking modal
│                              │
│      ( animated ring )       │  current mode, "Auto" badge if running
│        Blue Chase            │
│                              │
│ [Solid][Chase][Twinkle][Fire]│  effect: segmented / scrolling chips
│ [Specials]                   │
│                              │
│  ● ● ● ● ● ● ● ●             │  colour swatches in real colours
│  ◐ ◐ ◐ ◐                     │  bicolour swatches (half/half)
│                              │  unavailable combos hidden, not greyed
│ ┌──────────────────────────┐ │
│ │         STROBE           │ │  big, bottom, thumb reach
│ └──────────────────────────┘ │
│   [ Off ]          [ Auto ]  │
└──────────────────────────────┘
```

The web UI makes you pick a tab, then read a word ("Green"). The app turns that into two quick choices: effect, then a colour swatch. Swatches are recognisable without reading, in the dark, at a glance.

**Palettes** (shortlists)

- A Palette is a named, ordered shortlist of modes (e.g. "Fireworks night", "Carnival parade", "Favourites"). Order only sets tile position; there is no "next".
- The tab shows palette chips across the top and the selected palette's tiles as a 2-column grid. Tap any tile to switch to that mode instantly. Reorder by drag in Edit.
- The star next to the mode name on Live adds or removes that mode from the currently selected palette, so building a palette happens while browsing.
- "Play full screen" turns the palette into a performance view: every tile as large as the screen allows, the active tile highlighted, Off / Strobe / Auto along the bottom. The screen stays awake (`isIdleTimerDisabled`) and a Live Activity starts.
- Keyboards and Bluetooth pedals (which present as keyboards) map keys 1 to 9 to the first nine tiles and Space to Strobe, via `.onKeyPress`. Keep palettes to nine or fewer if you want every tile on a key.

```
┌──────────────────────────────┐
│ Done   Fireworks night   ● BLE│
│ Now: Rio Spin                │
│ ┌────────────┐┌────────────┐ │
│ │Blue Chase 1││Rio Spin   2│ │  active tile outlined
│ └────────────┘└────────────┘ │
│ ┌────────────┐┌────────────┐ │
│ │Yel/Blu Tw 3││Rainbow    4│ │
│ └────────────┘└────────────┘ │
│        ... up to 9 ...       │
│ [Off]  [   STROBE   ] [Auto] │
└──────────────────────────────┘
```

**Settings** (from the toolbar, not a tab): transmitter pairing, WiFi fallback credentials, haptics on/off, catalogue version, "Strobe repeat rate".

### 5.3 Interactions that change because it's native

| Interaction | Web today | App |
| --- | --- | --- |
| Confirming a command landed | Button turns active when TX echoes | Same visual, plus a light haptic tick when the TX acknowledges; warning haptic if it doesn't within 300 ms |
| Strobe | Tap = one strobe | Tap = one strobe with a heavy haptic. Press-and-hold repeats at a set rate (useful for rolls). |
| 999 | Modal dialog: "Activate?" then confirm | Press-and-hold for ~0.6 s with a filling ring and a haptic at completion. Just as safe against accidental taps, much faster, no dialog to dismiss. |
| Disconnection | Full-screen "Disconnected" modal blocks everything | Status pill turns amber, controls dim, reconnect happens silently; a haptic when it comes back. Commands are not queued: a stale "Blue" sent 20 s late is worse than nothing. |
| Auto | Button sends -1, never shows active | Auto badge on the ring, Auto button stays highlighted, shows the mode Auto picked |
| Hands busy | Must unlock phone, find the tab | Action Button, lock screen, watch, foot pedal |

An honest limit: the app can confirm that the **TX** accepted a command. It cannot confirm that drums received it, because RF24 auto-ack is off and receivers never transmit. The UI must not imply otherwise ("Sent", not "Delivered").

### 5.4 System integration (App Intents)

Define intents once in `DrumLightsKit` and reuse them everywhere:

- `SetModeIntent(mode)`, `PaletteSlotIntent(palette, slot)`, `StrobeIntent`, `AutoIntent`, `OffIntent`.
- They conform to `LiveActivityIntent` so they run in the app's process, which holds the BLE connection in the background.

Surfaces that come almost free from those intents:

- **Action Button** (iPhone 15 Pro and later): map to Strobe, or to one go-to palette slot. Press without looking.
- **Control Centre controls** (iOS 18): Strobe, Off, and a few user-chosen palette slots. Reachable from the lock screen with one swipe.
- **Live Activity** during Play: lock screen shows the palette name, the current mode, the first four palette tiles as buttons and Strobe; the Dynamic Island shows the current mode's colours.
- **Shortcuts**: lets people automate, e.g. "when I connect to the band's minibus Bluetooth, open Drum Lights".

### 5.5 Apple Watch

The watch is the most interesting surface for a mestre or drummer whose hands are occupied.

- Screen: the selected palette as a 2x2 tile grid (Digital Crown scrolls to more tiles), the active tile outlined, Strobe below.
- Double Tap gesture (Series 9 / Ultra 2 and later, `handGestureShortcut`) bound to Strobe: pinch index finger and thumb. Test it while holding a stick before promising it works; it may not.
- Transport: connect directly to the TX over BLE while the watch app is frontmost (lowest latency); fall back to WatchConnectivity relaying through the phone (adds roughly 100 to 500 ms, fine for switching tiles, poor for strobe timing).

## 6. Web UI: stays, and gets better

The web UI remains the only option for Android and for anyone who just walked up. It should keep parity on the core job (pick a mode, strobe, off, auto) and pick up the fixes that cost nothing:

- Render from `modes.json` (section 4), with colour swatches like the app.
- Show real state on connect and show Auto as active (needs the firmware fixes in 3.5).
- Replace the blocking "Disconnected" modal with a status banner.
- Hold-to-confirm for 999 instead of a modal.
- Keyboard shortcuts (1 to 9, Space for Strobe), so a Bluetooth pedal or keyboard works on the web too.
- Optional: a single palette in `localStorage` (they survive on the same phone, though the captive-portal browser may clear them; say so in the UI).

Forward-looking option: the BLE service also unlocks **Web Bluetooth on Android Chrome**. A copy of the web UI hosted on a normal HTTPS site (e.g. GitHub Pages) could talk to the TX over BLE, giving Android operators the "no WiFi switching" benefit without a native Android app. Safari does not support Web Bluetooth, so this does nothing for iOS.

## 7. Risks, ranked, and how to retire them

1. **WiFi AP + BLE + webserver on one ESP32.** RAM, flash partition size and WiFi/BT coexistence (they share one 2.4 GHz radio by time-slicing). Retire with a one-day spike: add NimBLE with the three characteristics to `TX/src/main.cpp`, check free heap and flash, then hammer both the web UI and a BLE scanner app (nRF Connect) at once for 30 minutes. If it's unstable, the app is not worth building; go to the web improvements only.
2. **2.4 GHz crowding near the RF24 link.** RF24 defaults to channel 76 (2.476 GHz). BLE hops across 2.402 to 2.480 GHz. BLE adaptive frequency hopping avoids busy channels, and the TX already retransmits 5 times, but measure: run a strobe-heavy test with BLE connected and count missed changes on a few receivers. Note: an audience full of phones is already doing this to you.
3. **Bonding management.** Losing a phone means clearing bonds. Add a "hold BOOT button 5 s to clear BLE pairings" routine to the TX.
4. **Distribution expiry.** Ship through the App Store, not TestFlight alone.
5. **Firmware drift across bands.** Other builders run different TX versions. The `proto` and `catalogue` numbers let the app degrade gracefully instead of sending modes the TX doesn't know.

## 8. Delivery plan

| Phase | Scope | Rough effort | Output |
| --- | --- | --- | --- |
| 0 | TX fixes from 3.5, `applyMode()` refactor, state on connect, `modes.json`, web UI renders from it | 1 to 2 days | Better web UI, foundation for app |
| 1 | BLE spike (risk 1 and 2) | 1 day | Go / no-go for the app |
| 2 | TX BLE service with passkey bonding; app Live tab over BLE, WebSocket fallback; haptics | 1 to 2 weeks (hobby pace) | Usable app |
| 3 | Palettes, Play view, keyboard/pedal support, App Intents, Action Button, Control Centre, Live Activity | 1 week | The features that justify the app |
| 4 | Watch app | 3 to 5 days | Hands-busy control |

Phase 0 is worth doing even if nothing after it happens.

## 9. Testing

- `DrumLightsKit` unit tests: catalogue parsing, protocol encode/decode, palette editing, reconnect logic against `MockTransport`.
- SwiftUI previews and the Simulator run on `MockTransport` (same idea as the web UI's dev-mode echo server).
- BLE needs a real device and a TX on the bench; keep one spare TX and two receivers as a test rig.
- Before each season: a rehearsal using a palette with the screen locked, switching only from the lock screen, Action Button, watch and pedal.

## 10. Later ideas (deliberately parked)

- **Animated ring preview** of each effect in SwiftUI `Canvas`, so swatches show motion, not just colour. Nice, not essential.
- **Tempo and brightness** using the spare RF payload bytes (3.6): tap tempo in the app sets chase speed on new receivers while old ones ignore it.
- **Drum groups** (surdos blue, repiques yellow) using a group mask in the spare bytes. The app's effect × colour picker extends naturally to "per section".
- **Beat-reactive strobe** from the phone microphone. Latency and a very loud environment make this a research project, not a feature.
