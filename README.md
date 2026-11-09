# Drum Lights

## About

This project provides synchronised, colour-changing, remote-control LED lights for a drum band (but could be used for other similar applications). It was first created for [Swan Samba](https://www.swansamba.co.uk) to use for evening/winter performances.

![Swan Samba at Winchester Fireworks](_docs/swan-samba-winchester-fireworks.jpg)

Hundreds of hours have gone into developing this project - if you make use of it and wish to thank me, then please consider a donation;

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/phildye)

Contributions to improve this documentation or code would also be very welcome to help other users.

## Skills & Requirements

This is a technically complex project! Whilst I've partly documented it here, to use it successfully you'll need to improvise a bit and will need some experience with;

- sourcing and assembling the electronic components (eg from AliExpress), including soldering techniques

- building and uploading the firmware using [Visual Studio Code](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/)

- 3D-printing for the enclosure boxes

If you are not confident with these skills, then this probably isn't the project for you; I'm happy to answer the odd question, but this project is provided 'as-is' and as a hobby I'm unable to provide extensive support, sorry!

## Overview

The system consists of three parts;

- Transmitter (TX) - receives commands from a controller and broadcasts a radio signal to the receivers.

- Receiver (RX) - receives the broadcast radio signal and drives a strip of individually-addressable LED pixels mounted inside the shell of each drum.

- Controller - the phone used to pick colours/patterns. On an iPhone this is the native [iOS app](#ios-app) over Bluetooth LE; on anything else (or as a fallback) it's the [web UI](#web-ui-fallback) served by the TX over its own WiFi hotspot. Both can be used at once and stay in sync.

Both TX and RX are powered by portable USB battery packs ('power banks'), typically carried in a pocket by each band member (or could be mounted to each drum).

## Transmitter

The transmitter (TX) consists of an ESP32-S3 dev-kit board plus a RF24-based radio module, mounted on proto-board. The original design used a classic ESP32 dev-kit; that build is still supported (web UI only, see below) but new transmitters should use the S3, because the classic ESP32's Bluetooth connects too slowly and unreliably for live use (8/10 connections at 4.4s, against 20/20 at 1.0s on the S3; see #19).

Although the RF24 modules use the 2.4Ghz ISM radio band, they are not WiFi; this was chosen for low-latency and avoiding the need for a network router for a large number of receivers (which would not be able to connect simultaneously to the ESP32 hotspot).

Significantly better results have been found with the E01-2G4M27D module, rather than the cheaper basic nRF24L01-PA-LNA which suffers RF interference and instability.

The RF24 module uses a configurable transmit power with multiple channels available, but does not avoid packet collision with other sources using the same frequency. For this reason we retransmit each command multiple times in quick succession, in the hope that 'one gets through'. It also requires a stable 3.3v supply, which at high-power transmission could exceed that available from the ESP32, so a separate buck converter is used fed from the power supply.

The TX offers two ways in for controllers, both feeding the same single mode-change path so every controller sees the same state;

- **Bluetooth LE** (ESP32-S3 only): a GATT service used by the iOS app, with State (read/notify), Command (encrypted, authenticated write) and Info (protocol, firmware and catalogue versions) characteristics. Phones pair once with a 6-digit PIN. Up to four controllers can be connected at once and eight pairings are remembered (the oldest is dropped when full). The full protocol is in [iOS/DESIGN.md](iOS/DESIGN.md).

- **WiFi hotspot + web UI**: the ESP32 broadcasts a WiFi hotspot (configured as a captive portal) running an AsyncWebServer that serves a HTML/CSS/JS page from LittleFS, with commands and state carried over a websocket at `ws://192.168.4.1/ws`.

The available effects and colours are defined once in [`TX/app/src/modes.json`](TX/app/src/modes.json). The web UI is rendered from it, the iOS app bundles a copy, and the TX reports its catalogue version so the app can warn about a mismatch. CI fails if the copies drift apart.

### Building the TX firmware

There are two PlatformIO environments in `TX/platformio.ini`:

- `esp32-s3` (default): an ESP32-S3 dev-kit (8MB or 16MB flash), with the web UI **and** the Bluetooth LE service. The RF24 goes on the S3's default SPI pins: SCK 12, MISO 13, MOSI 11, CS 10, CE 9. Serial is on the native USB port; if your cable is in the UART (COM) port, delete `-DARDUINO_USB_CDC_ON_BOOT=1` or the monitor stays silent.
- `az-delivery-devkit-v4`: the original classic ESP32 TX, web UI only (RF24 on SCK 18, MISO 19, MOSI 23, CS 5, CE 17).

Create `TX/src/secrets.h` (it's git-ignored):

```cpp
#define WIFI_SSID "Drum Lights"
#define WIFI_PASS "your-wifi-password"
#define BLE_PASSKEY 482916          // 6-digit Bluetooth PIN; no leading zero
// #define BLE_NAME "Swan Samba TX" // optional, so neighbouring bands don't collide
```

`BLE_PASSKEY` is required for the S3 build; it won't compile without it.

Then build the web UI into `TX/data` and upload both the filesystem and the firmware:

```sh
cd TX/app
npm ci
npm run build              # writes TX/data
cd ..
pio run -t uploadfs        # LittleFS image with the web UI
pio run -t upload          # firmware
```

Add `-e az-delivery-devkit-v4` to the `pio` commands for a classic ESP32 TX.

**Hold the BOOT button for 5 seconds** to forget every Bluetooth pairing (the LED flashes three times), e.g. after losing a phone. Paired iPhones then need "Forget This Device" in their Bluetooth settings before they can pair again.

## Controlling the lights

### iOS app

The native iPhone app in [`iOS/`](iOS/README.md) is the preferred controller. It talks to the TX over Bluetooth LE, so the phone keeps its normal internet connection and never has to join the hotspot. It also gives things the web page can't:

- haptic confirmation of each tap, so the operator can keep their eyes on the band;
- staying connected with the screen locked, and reconnecting by itself when the TX comes back into range;
- remembering "its" TX, so it won't grab a neighbouring band's transmitter.

The app shows "Sent to TX" rather than "Delivered": the TX confirms it accepted a command, but nothing can confirm the drums received it. While disconnected the controls dim and ignore taps, rather than queuing stale commands.

Currently the Live tab and Settings work. Saved palettes, Live Activity / lock-screen controls, App Intents (Action Button, Shortcuts) and Apple Watch are planned; see [iOS/DESIGN.md](iOS/DESIGN.md) and #20. Build and test instructions are in [iOS/README.md](iOS/README.md). It needs iOS 17 or later; in the Simulator it drives a pretend TX.

### Web UI (fallback)

The web UI remains a first-class controller for Android phones, anyone without the app, and for classic-ESP32 transmitters that have no Bluetooth. Connect to the TX's WiFi hotspot and the captive portal opens the control page (or browse to `http://192.168.4.1`).

![TX UI](_docs/TX-UI.png)

The iOS app can also fall back to it: with *Settings › WiFi fallback* turned on, if Bluetooth hasn't connected within 6 seconds the app joins the hotspot itself and uses the same websocket, then leaves again once Bluetooth is back. This needs the Hotspot Configuration capability, which requires a paid Apple Developer Program membership.

The trade-offs of the web UI are the reason the app exists: the phone loses its internet while on the hotspot, iOS may nag about "no internet connection", and the connection drops when the screen locks.

## Receiver

The receiver (RX) consists of an ESP8266-based Wemos D1-mini clone board, plus a RF24 module, mounted on a PCB.

![RX schematic](_docs/RX-schematic.png)

Better reliability has been found with the E01-ML01DP4 module, rather than the cheaper basic nRF24L01. 

It receives radio commands from the TX and uses these to drive a strip of WS2812B LEDs ("neopixels") using the excellent [FastLED](https://fastled.io) library.

Best-practice often requires the use of a 3.3v level-shifter and/or resistor in the data line, but testing showed this not to be neccessary, probably given the short data-line length, although a capacitor _is_ provided on the power supply (although as a DC source, USB battery packs are probably sufficiently stable already). 

The receiver reads a configuration file with the number of LEDs available in the attached strip, and uses this to dynamically compute moving effects to suit the varying drum sizes within a band. This file should be uploaded to the filesystem on each board.

## LED mounting

Self-adhesive IP65 WS2812B LED strip is mounted inside the shell of each drum, approx a quarter of the way from the top, around the full circumference. 

The strip requires 3-core cable: 5V, data and ground;
![WS2812B strip with wires attached](_docs/IMG_3530.jpeg)

Ensure this is sealed with silicone or hot-glue to prevent water ingress, and insulated with heatshrink;
![WS2812B strip with heatshrink](_docs/IMG_3531.jpeg)

The cable exits through the sound hole/port and runs to an enclosure holding the RX board.

## Enclosures

Both RX and TX are mounted in custom 3D-printed boxes; the RX in particular is designed to secure the components against vibration. Power is supplied via a 5.5mm DC jack using a [USB cable](https://www.kenable.co.uk/en/usb-cables-adapters/usb-20-cables/usb-20-adapters/7629-usb-to-dc-power-cable-usb-20-for-21mm-x-55mm-5v-2a-2000ma-1m-007629-5055383476299.html).

Current designs are in [`STL/`](STL/) as FreeCAD sources plus printable STL/STEP files: the v4.1 RX box and lid, the original (2023) TX box for the classic ESP32, and the v5 TX box and lid for the ESP32-S3.

![3D-printed enclosure mounted on a surdo drum](_docs/IMG_3526.jpeg)

Experience has shown it's best to mount the RX on the *rear* of all drums with 3M VHB double-sided tape. Ensure the TX is positioned at the rear of the band too, to avoid metal-shelled drums blocking the RF signal. _Ensure players know this to mount their drum accordingly!_

## PCBs

After breadboard prototyping, initial builds were built on protoboard but for more than a small handful of boards (which require plenty of soldering!), it's advisable to consider custom-printed PCBs. 

![Custom PCB](_docs/PCB.jpg)

[Design files are provided](PCB/) for [KiCAD](https://www.kicad.org/) with Gerber output files ready for use at [JLCPCB](https://jlcpcb.com/). The PCB is for the RX only; the TX (either board) is built on proto-board.

## Effects

The lighting patterns currently available are;

- Static: every pixel the same colour, available in various colours.
- Chase: 4 quadrants of moving pixels with a fading 'tail', rotating around the drum, available in various colours.
- Twinkle: a number of pixels randomly light then fade, available in various colours.
- Fire: a 'flickering' flame effect in varying colours.
- 'Rio Spin': 3 segments of solid colour (blue/green/yellow) rotating around the drum.
- 'Rio Disco': twinkle, with blue/green/yellow only.
- 'Rio Flag': a blue/green/yellow sequence representing the Brazilian flag, rotating around the drum.
- Rainbow: a classic FastLED multi-colour effect, rotating around the drum.
- Hazards: 2 segments of orange at the side of each drum, designed to imitate a vehicle's hazard lights when stopped.
- Strobe: rapid, short-duration flashes of full-intensity white - perfect for big hits!
- 999: alternate high-frequency flashing blue strobes (named after the UK emergency-services telephone number).
- Auto: randomises most of the above every 30s; ideal to 'fire-and-forget' if no-one is available to run the show.



Many of the FastLED demo effects suit 1D (strip) or 2D (panel) setups; the circular/ring configuration here does present some additional challenges, but further effects are really only limited by imagination (although compexity of the control UI should be considered).

## Synchronicity

Currently, all drums receive the same command and hence display the same colour/pattern at the same time. The speed of the rotating patterns (ie RPM) is not synchronised across varying diameter drums; early experiments showed this to be hard to maintain a smooth frame rate at slower speeds (on smaller drums); temporal dithering may help this.

It would be relatively simple to use addressing or channel features of the RF24 to control each drum type seperately, allowing complex displays and patterns 'across' the band. The main obstacle is likely to be the complexity of the control UI.