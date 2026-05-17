# RX: SPIFFS → LittleFS Migration

**Status:** Approved
**Date:** 2026-05-17
**Scope:** `RX/` firmware only (ESP8266 / ESP12E). TX is already on LittleFS.

## Goal

Move the RX firmware off SPIFFS (deprecated in the ESP8266 Arduino core) onto LittleFS, while preserving the per-unit `/config.ini` already stored on flash for each deployed drum. The user must not have to re-create or re-upload config files per unit.

## Constraints

- SPIFFS and LittleFS occupy the same flash region but use incompatible on-disk formats. A LittleFS mount on a SPIFFS-formatted area fails, and formatting LittleFS would wipe the existing config. Migration must read SPIFFS data *before* LittleFS formats.
- `yurilopes/SPIFFSIniFile` is hard-coded to SPIFFS and must be removed.
- `stevemarple/IniFile` (the upstream) is hard-coded to SD / SdFat and is not a drop-in replacement.
- Each deployed drum has a distinct `/config.ini` (different `[leds] count` and `[drum] type` values per drum).

## Approach

Three discrete pieces, in `RX/` only.

### 1. `platformio.ini`

Add `board_build.filesystem = littlefs`. Remove the `yurilopes/SPIFFSIniFile` dependency. No other changes.

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
board_build.filesystem = littlefs
monitor_speed = 115200
lib_deps =
    nrf24/RF24@^1.4.10
    fastled/FastLED@^3.8.0
monitor_filters = esp8266_exception_decoder
```

### 2. First-boot migration

Runs in `setup()` before any config reading. Pseudocode:

```
if LittleFS.begin():
    // already migrated (or first flash with no prior SPIFFS data)
    proceed
else:
    buffer = empty
    if SPIFFS.begin():
        if SPIFFS.exists("/config.ini"):
            buffer = read entire /config.ini into RAM
        SPIFFS.end()
    LittleFS.format()
    if not LittleFS.begin():
        ledMode = -2  // file-failure error state
        // fall through; defaults will be used for numLeds / drumType
    else if buffer not empty:
        write buffer to /config.ini on LittleFS
```

Properties:
- Idempotent: after a successful migration, subsequent boots take the first branch and never touch SPIFFS.
- Self-healing: a unit that was never flashed with the new firmware before will still boot — it just starts with an empty filesystem and uses code defaults.
- Bounded RAM: the config file is tiny (<1 KB in practice). Use a fixed 512-byte buffer; abort migration with an error if larger.

### 3. Tiny custom INI parser

Replaces all `SPIFFSIniFile` call sites. Implemented as a single function in `main.cpp`:

```cpp
bool readConfig(const char* path, int& numLeds, int& drumType);
```

Behavior:
- Opens `path` on `LittleFS`.
- Single-pass line-oriented scan.
- Trims whitespace; skips blank lines and lines beginning with `;` or `#`.
- Tracks `[section]` headers.
- For each `key = value` line, matches against the known set:
  - `[leds] count` → writes to `numLeds`
  - `[drum] type` → writes to `drumType`
- Unknown keys/sections are silently ignored (forward-compatible with future config additions).
- Returns `true` if the file was opened (regardless of which keys were present); `false` if the file is missing.

Defaults if the file is missing or a key is absent: the existing initial values in `main.cpp` (`numLeds = MAX_LEDS`, `drumType = 0`). Same behavior as the current SPIFFSIniFile path when a key isn't found.

## Error handling

| Condition | Behavior |
|---|---|
| LittleFS mount + format both fail | `ledMode = -2` (existing "file failure" indicator — DarkMagenta error blink) |
| `/config.ini` missing on LittleFS | Use code defaults, no error LED |
| Specific key missing in config | Use code defaults for that key, no error LED |
| SPIFFS-side read fails during migration | Continue with empty buffer; LittleFS still gets formatted; unit uses defaults |

This mirrors the existing tolerance: today the firmware also falls through to defaults if a key is absent.

## Files touched

- `RX/platformio.ini` — `lib_deps` + `board_build.filesystem`
- `RX/src/main.cpp` — remove `#include <SPIFFSIniFile.h>`, add `#include <LittleFS.h>`, replace the `#pragma region CONFIGFILE` block with the migration + `readConfig()` call, add `readConfig()` definition
- `RX/data/example.ini` — unchanged (format is compatible)

No changes to `chase.cpp`, `effects.cpp`, `fire.cpp`, `strobes.cpp`, `twinkle.cpp`, or `prototypes.h`.

## Explicitly out of scope

- Adding new config keys
- Changing the existing error indication scheme
- Removing the SPIFFS-read migration code (kept as a safety net; can be stripped in a follow-up release after all drums are confirmed migrated)
- Touching TX-side code

## Verification

- **Build:** PlatformIO `pio run -e esp12e` in `RX/` must succeed with no warnings introduced by the change.
- **Hardware (user-driven, cannot be done from this environment):**
  - First boot on a drum with existing SPIFFS config preserves `numLeds` and `drumType` (serial monitor prints expected values).
  - Second boot uses the LittleFS-only fast path (no SPIFFS mount attempted).
  - A drum with no prior config falls back to defaults without error LED.
