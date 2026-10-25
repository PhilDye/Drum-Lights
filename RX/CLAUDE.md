# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Context

This is the **receiver (RX)** firmware of the Drum Lights project (repo root is `..`). Each RX is an ESP8266 (Wemos D1-mini clone, built as `esp12e`) with an nRF24L01-family radio, driving a ring of WS2812B LEDs mounted inside a drum. A single **transmitter** (`../TX`, ESP32) serves a web UI over a WiFi captive portal and broadcasts the selected mode to all receivers by RF24. Hardware design lives in `../PCB` (KiCAD + Gerbers) and enclosures in `../STL`; the project overview is `../README.md`.

## Commands (PlatformIO, run from this directory)

```sh
pio run                       # build
pio run -t upload             # flash firmware
pio run -t uploadfs           # flash data/ to LittleFS (required: config.ini)
pio device monitor            # serial @ 115200, with esp8266_exception_decoder
```

`platformio.ini` pins `upload_port`/`monitor_port` to a machine-specific `/dev/cu.usbserial-*` path; adjust or override with `--upload-port` for a different board. There are no tests (`test/` is the PlatformIO placeholder).

## Per-board config

`data/config.ini` (gitignored, one per drum; `data/example.ini` is the template) is read from LittleFS at boot by `loadConfig()` (`src/config.cpp`) and parsed by `IniConfig` (`src/ini.cpp`):
- `[leds] count` — number of LEDs on this drum's strip (1 to `MAX_LEDS` = 104; out-of-range values fall back to `MAX_LEDS` with error mode -2). Effects scale to this, so ring sizes vary per drum.
- `[drum] type` — read but currently unused.

The INI format must not change: boards in the field keep the file they were set up with. `IniConfig` deliberately reproduces the parsing rules of the SPIFFSIniFile library it replaced (case-insensitive names, `;`/`#` comments, `atoi` values, its 78-character line limit).

**Legacy SPIFFS migration:** boards set up before the move to LittleFS still have a SPIFFS partition. LittleFS is mounted with auto-format **off** (the default would wipe that partition). If it fails to mount, `loadConfig()` reads `config.ini` from SPIFFS (also with auto-format off), and only if that copy is valid reformats the partition as LittleFS and writes the file back byte-for-byte, logging the contents to serial first. Other files on the old SPIFFS partition are not carried over. The SPIFFS fallback (`readLegacySpiffs`) can be deleted once every board has booted this firmware once.

## Architecture

- **Protocol:** the TX sends a single `int` mode number on pipe address `"RxAA1"`, auto-ack off, retransmitted several times (`RETRANSMITS` in `../TX/src/main.cpp`) because there is no collision avoidance. The radio address, payload type and mode numbers must stay in sync between `src/main.cpp` and `../TX/src/main.cpp` (plus the UI in `../TX/app`).
- **Main loop** (`src/main.cpp`): `readRadio()` sets the global `ledMode` and clears the strip; a big `switch (ledMode)` dispatches to an effect function each frame; then `FastLED.show()` + `FastLED.delay()` at `FRAMES_PER_SECOND`. Effects are therefore called once per frame and must be non-blocking, keeping animation state in function-local `static` variables (and `EVERY_N_MILLIS` etc.).
- **Mode numbering** (documented in `src/prototypes.h`): 1–8 solid colours, 11–18 single-colour chase, 21+ bicolour chase, 50–53 fire palettes, 61+ bicolour twinkle, 81–88 single twinkle, 91–93 Rio effects, 97 hazards, 98 one-shot strobe (reverts to the previous mode; it fires once per retransmit), 99 rainbow, 199 "999" blue strobe. The colour order within each block (green, yellow/gold, blue, red, white, cyan, magenta/indigo, orange) is a convention shared with the TX UI. "Auto" mode is implemented on the TX, which just sends random modes.
- **Status/error modes** (negative): -1/0 idle blinking green pixel, -2 filesystem/config failure (magenta blink), -3 radio failure (red blink); unknown mode → grey blink. These blink on pixel 1 via `showStatus`/`showError`, which block with `FastLED.delay` (unlike effects). Error modes are fragile: a successful radio init in `setup()` resets `ledMode` to -1, which hides a -2, and after `IDLETIMEOUT` (30s) *any* negative mode, errors included, is replaced by mode 63.
- **Adding an effect:** implement in a `src/*.cpp` file (effects are grouped: `chase.cpp`, `twinkle.cpp`, `fire.cpp`, `strobes.cpp`, `effects.cpp`), declare it in `src/prototypes.h` (which also holds the fire palettes), add a `case` in `main.cpp`, and add the matching button/mode on the TX side.

## Hardware constants

Radio CE=GPIO4, CSN=GPIO15; LED data on GPIO2; power limited via `FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000)`. `FASTLED_ALLOW_INTERRUPTS 0` is defined before including FastLED.

## Style

`.editorconfig`: LF, UTF-8, final newline, 2-space indent for C/C++.
