# Drum Lights iOS app

A native controller for the TX that talks Bluetooth LE, so the phone keeps its normal internet. The design and the reasons for it are in [DESIGN.md](DESIGN.md), and the clickable mockup is in [mockup/](mockup/README.md).

Status: phase 2 of [#17](https://github.com/PhilDye/Drum-Lights/issues/17), tracked in [#20](https://github.com/PhilDye/Drum-Lights/issues/20). The Live tab and Settings work. Palettes, Play, Live Activity, App Intents and the watch come in later phases.

## Layout

| Path | What |
| --- | --- |
| `DrumLights.xcodeproj` | The app project (Xcode 16 or later). Its groups follow the folders, so new files in `DrumLights/` are picked up without editing the project. |
| `DrumLights/` | The SwiftUI app: Live tab, Settings sheet, haptics, settings storage |
| `Config/` | `Info.plist` additions (Bluetooth and local-network wording, `bluetooth-central` background mode) and the entitlements (Hotspot Configuration) |
| `DrumLightsKit/` | Swift package shared by the app and, later, the widgets and watch: catalogue, protocol, transports, `LightsController`, tests |

## Building and running

1. Open `iOS/DrumLights.xcodeproj` in Xcode.
2. Choose your team under Signing & Capabilities. A free (Personal Team) account works: the Hotspot Configuration capability is **temporarily switched off** so the app can be signed without a paid Apple Developer Program membership. Until it's back, the WiFi fallback can't join the TX hotspot by itself; join it in the iOS Settings app first. To restore it with a paid account, revert the commit "iOS: switch off the Hotspot capability for free accounts" (it puts `CODE_SIGN_ENTITLEMENTS = Config/DrumLights.entitlements` back in both target configurations and the original Settings wording).
3. Run on an iPhone (iOS 17 or later) near a TX.

In the Simulator, which has no Bluetooth, the app drives a pretend TX (`MockTransport`): it connects, echoes commands, runs Auto, and shows "Connected · Demo".

## How it talks to the TX

- **Bluetooth first.** The app scans for the Drum Lights GATT service, remembers the first TX it connects to, and from then on only uses that one. iOS keeps the connection with the screen locked and reconnects by itself when the TX comes back into range.
- **Pairing.** The TX asks for its 6-digit PIN the first time. iOS shows that prompt itself and remembers the TX afterwards. "Forget transmitter" in Settings makes the app look for any TX again; iOS doesn't let apps remove a Bluetooth pairing, so to drop the PIN too, forget the TX in the iOS Settings app under Bluetooth.
- **WiFi fallback** (off by default; Settings › WiFi fallback). If Bluetooth hasn't connected for 6 seconds, the app joins the TX hotspot and uses the existing WebSocket. It leaves the hotspot again once Bluetooth is back. This also works with TX firmware that has no Bluetooth yet.
- **No queued commands.** While disconnected the controls dim and ignore taps. A command from 20 seconds ago is worse than none.
- **"Sent to TX", never "Delivered".** The TX confirms it accepted a command (BLE write response, or its WebSocket state echo); nothing can confirm the drums received it. A light haptic means the TX accepted it, and a warning haptic means it didn't within 300 ms.

## Firmware this expects

The Bluetooth service is in the `esp32-s3` TX build ([#44](https://github.com/PhilDye/Drum-Lights/pull/44)); see "Building the TX firmware" in the root README for the PIN. The classic ESP32 build has no Bluetooth, so with it the app needs the WiFi fallback.

- The TX starts pairing as soon as a phone connects, so the PIN prompt comes up on connect, not on the first tap.
- If the TX's pairings are cleared (BOOT held for 5 s), the iPhone must "Forget This Device" in Settings › Bluetooth before it can pair again.
- The TX ignores Command writes that aren't exactly 4 bytes. The app's `Command` has an optional parameter block (`DESIGN.md` section 3.6) but sends only the 4-byte mode; sending parameters needs the TX to accept longer writes first.
- Info is read once per connection; its `catalogue` drives the mismatch warning in Settings.

## Tests

```sh
cd iOS/DrumLightsKit
swift test
```

The tests cover catalogue parsing (including a check that the bundled `modes.json` matches `TX/app/src/modes.json`), the BLE and WebSocket encodings, and `LightsController` against `MockTransport`: acknowledgements and haptics, no queuing while disconnected, staying in step with other controllers, Auto through Strobe, strobe repeat, and the Bluetooth/WiFi handover.

`WebSocketTransportTests` runs only when `DRUMLIGHTS_WS_URL` is set. Point it at a real TX (it selects Off), or at the stand-in in `DrumLightsKit/Tools/fake_tx.py`:

```sh
pip install websockets
python3 Tools/fake_tx.py &
DRUMLIGHTS_WS_URL=ws://127.0.0.1:8765/ws swift test
```

It needs macOS: Foundation on Linux has no WebSocket client. The rest of the package tests run on Linux too. CI (`.github/workflows/ios-build.yml`) runs everything on macOS, including the fake TX, and builds the app for the Simulator.

## Updating the catalogue

`DrumLightsKit/Sources/DrumLightsKit/Resources/modes.json` is a copy of `TX/app/src/modes.json`. After changing the original, copy it over; a test fails until you do.
