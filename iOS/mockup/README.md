# iOS app mockup (source snapshot)

This is a snapshot of the clickable design mockup for the iOS controller app, so the design survives independently of the hosted copy. The design doc is [`../DESIGN.md`](../DESIGN.md), and the work is tracked in [PhilDye/Drum-Lights#17](https://github.com/PhilDye/Drum-Lights/issues/17).

- Live, clickable version: <https://claude.ai/artifact/VGrhABAfKQD2wLGPz6Crdq> (a Claude "Design" canvas; private to the owner until shared).
- Snapshot taken: 2026-10-25, from canvas version `1792934794-c9ae`.

If the hosted mockup and these files disagree, check which is newer and update this folder.

## What's here

| File | Frame | Notes |
| --- | --- | --- |
| `Main.dc.html` | iPhone app, 390×844, **interactive** | Live tab, Palettes tab, Play full screen, Settings sheet, plus in-app previews of the lock-screen Live Activity and the Watch, both driven by the selected palette |
| `LockScreen.dc.html` | Lock screen, 390×844, static | Example Live Activity for the "Fireworks night" palette |
| `Watch.dc.html` | Apple Watch, 300×366 (scaled up), static | Example watch screen for the same palette |
| `canvas.json` | Canvas layout | Frame positions and titles |

## How to read the files

Each `.dc.html` file is a self-contained "Design Component":

- The markup inside `<x-dc>` uses `{{name}}` placeholders, `<sc-for>` loops and `<sc-if>` conditionals.
- The `<script type="text/x-dc">` block holds the logic. Its `renderVals()` returns the values and click handlers the markup uses.
- `support.js` is the hosting canvas's runtime and isn't included here, so the files won't render if you open them directly in a browser. Use the hosted link to click through, and read the source for exact values.

All styling is inline, so sizes, colours and spacing can be read straight off each element.

## Things to lift directly when building the app

The `Component` class in `Main.dc.html` doubles as an executable spec:

- **`cat()`**: the mode catalogue. Colour names and hex values, the colour order used by the web UI, the mode number for every effect and colour (solid 1 to 8, chase 11 to 18 and 21 to 28, twinkle 81 to 88 and 61 to 68, fire 50 to 53, specials 91/92/93/97/99, 999 = 199, Off = 0), and the Auto list. It mirrors `RX/src/main.cpp` and should become `modes.json` (`DESIGN.md` section 4).
- **`dots(m)` and `ringClass(m)`**: how each effect is previewed on a 24-dot ring.
  - Chase: 4 quadrants with fading tails, rotating.
  - Twinkle and Rio Disco: staggered fade.
  - Fire: flicker.
  - Rio Spin: 3 rotating segments.
  - Rio Flag: a rotating sequence.
  - Rainbow: rotating hues.
  - Hazards: two side segments blinking.
  - 999: alternating halves.
  - Strobe: all dots flash white.
- **Auto rules**: `setMode()` stops Auto for every mode except Strobe, matching [PhilDye/Drum-Lights#15](https://github.com/PhilDye/Drum-Lights/issues/15). In the mockup Auto changes every 5 s; the real TX changes every 30 s.
- **Timings**: Strobe repeats every 300 ms while held; 999 needs a 600 ms hold; "Sent to TX" shows for 1.2 s.
- **Palettes**: the shape is `{ name, ids: [mode numbers] }`. The star on Live adds the current mode to, or removes it from, the selected palette.
- **Live Activity status line** (`lockSub`):
  - "Auto is choosing"
  - "Not in this palette"
  - "Tap a tile to switch" (the mode is one of the first four tiles)
  - "Tile N in this palette"
- **Disconnected state**: the status pill turns amber ("Reconnecting…"), and controls drop to 35% opacity and ignore taps.

Colours follow the iOS dark-mode system palette:

| Colour | Hex |
| --- | --- |
| Background | `#000` |
| Grouped surfaces | `#1C1C1E`, `#2C2C2E` |
| Secondary text | `#AEAEB2` |
| Accent | `#0A84FF` |
| Strobe | `#FF9F0A` on black |
| Connected | `#30D158` |

## Differences from the ASCII sketches in DESIGN.md

The Live tab was hand-tuned on the canvas after the doc's sketch was written:

- The ring is scaled down and sits to the left of the mode name.
- The "Mode N" subtitle was removed.
- Swatches are 60 px and fill the available height.
- Two-colour swatches use the same 4-column grid as single colours.
- Effect tabs are 44 px tall.

Where these files and the doc's sketches differ on layout, these files are the more recent.

The tiles' slot numbers (1 to 9) in `Main.dc.html` are left over from a dropped keyboard/pedal idea: there is no keyboard or pedal control, so leave them out of the app.

The example palettes ("Fireworks night", "Carnival parade", "Favourites"), the date and time, and the placeholders `[YOUR SSID]` and `[TX VERSION]` are sample content.
