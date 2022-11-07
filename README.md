# Drum Lights

## About

This project provides synchronised, colour-changing, remote-control LED lights for a drum band (but could be used for other similar applications).

It was first created for [Swan Samba](https://www.swansamba.co.uk) to use for evening/winter performances, after previously using sets of random, multi-colour LED strips mounted underneath each drum. These were good, but did not provide the synchronised effects desired.

## Overview

The system consists of two components;

- Transmitter - provides a web-based interface to select required colours/patterns, and broadcasts a radio signal.

- Receiver - receives the broadcast radio signal and drives a strip of individually-addressable LED pixels mounted inside the shell of each drum.

Both are powered by portable USB battery packs ('power banks'), typically carried in a pocket by each band member (or could be mounted to each drum).

## Transmitter

The transmitter (TX) consists of an ESP32 dev-kit board plus a nRF24L01+PA+LNA radio module, mounted on proto-board.

Although the nRF24L01 modules use the 2.4Ghz ISM radio band, they are not WiFi (which is available on the ESP32 already); this was chosen for low-latency and avoiding the need for a network router for a large number of receivers (which would not be able to connect simultaneously to the ESP32 hotspot).

The ESP32 broadcasts a WiFi personal hotspot (configured as a captive portal) to allow a user who wants to control the display to connect simply from their mobile phone.

It runs an AsyncWebServer serving a simple HTML/CSS/JS page from a SPIFFS filesystem, with control commands received by websocket (allowing simultaneous control from multiple clients if required).

The nRF24L01+ module uses a configurable transmit power with multiple channels available, but does not avoid packet collision with other sources using the same frequency. For this reason we retransmit each command multiple times in quick succession, in the hope that 'one gets through'. It also requires a stable 3.3v supply, which at high-power transmission could exceed that available from the ESP32, so a seperate buck converter is used fed from the power supply.

## Receiver

The receiver (RX) consists of an ESP8266-based Wemos D1-mini clone board, plus a nRF24L01+ module, mounted on proto-board.

It receives radio commands from the TX and uses these to drive a strip of WS2812B LEDs ("neopixels") using the excellent [FastLED](https://fastled.io) library.

Best-practice often requires the use of a 3.3v level-shifter and/or resistor in the data line, but testing showed this not to be neccessary, probably given the short data-line length, although a capacitor is provided on the power supply (although USB battery packs are probably sufficiently stable already). 

The receiver reads a configuration file with the number of LEDs available in the attached strip, and uses this to dynamically compute moving effects to suit the varying drum sizes within a band.

## LED mounting

Self-adhesive LED strip is mounted inside the shell of each drum, approx a quarter of the way from the top, around the full circumference. The cable exits through the sound hole/port and runs to an enclosure.

## Enclosures

Both RX and TX are mounted in custom-designed 3D-printed boxes; the RX in particular is designed to secure the components against vibration. Power is supplied via a 5.5mm DC jack.

---

The following describes some of the design rationale.

## Effects

Effects currently available are;

- Static: every pixel the same colour, available in various colours.
- Chase: 4 quadrants of moving pixels with a fading 'tail', rotating around the drum, availa ble in various colours.
- Twinkle: a number of pixels randomly light then fade, available in various colours.
- 'Rio Spin': 3 segments of solid colour (blue/green/yellow) rotating around the drum.
- 'Rio Disco': twinkle, with blue/green/yellow only.
- 'Rio Flag': a blue/green/yellow sequence representing the Brazilian flag, rotating around the drum.
- Rainbow: a classic FastLED multi-colour effect, rotating around the drum.
- Hazards: 2 segments of orange at the side of each drum, designed to imitate a vehicles hazard lights when stopped.
- Strobe: rapid, short-duration flashes of full-intensity white.

Many of the FastLED demo effects suit 1D (strip) or 2D (panel) setups; the circular/ring configuration here does present some additional challenges, but further effects are really only limited by imagination (although compexity of the control UI should be considered).

## PCBs

After breadboard prototyping, initial builds were built on protoboard (pads, not vero-/stripboard); for more than a small handful of boards, it would be adviseable to consider a custom PCB.

## Synchronicity

Currently, all drums receive the same command and hence display the same colour/pattern at the same time. The speed of the rotating patterns (ie RPM) is not synchronised across varying diameter drums; early experiments showed this to be hard to maintain a smooth frame rate at slower speeds (on smaller drums); temporal dithering may help this.

It would be relatively simple to use addressing or channel features of the nRF24L01+ to control each drum type seperately, allowing complex displays and patterns 'across' the band. The main obstacle is likely to be the complexity of the control UI.