# RX LittleFS Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the RX firmware off SPIFFS onto LittleFS while preserving the per-unit `/config.ini` already stored on each deployed drum.

**Architecture:** Three pieces in `RX/` only — a tiny in-file INI parser, a one-time SPIFFS-read / LittleFS-format / config-rewrite migration helper that runs on first boot, and a `platformio.ini` swap to LittleFS. After the first boot, the migration path is dormant and the firmware behaves as a plain LittleFS application.

**Tech Stack:** Arduino framework on ESP8266 (ESP12E), PlatformIO build, ESP8266 Arduino core 3.x (provides both `SPIFFS` from `FS.h` and `LittleFS` from `LittleFS.h`).

**Spec:** [docs/superpowers/specs/2026-05-17-rx-littlefs-migration-design.md](../specs/2026-05-17-rx-littlefs-migration-design.md)

---

## Note on testing

This project has no host unit test framework configured (the `RX/test/` directory contains only the default README). The verification gate for each task is a clean PlatformIO build:

```
cd RX && pio run -e esp12e
```

If the build fails after a task, that is the failing-test signal — the task is not complete until it succeeds.

On-device verification (config preservation across the SPIFFS→LittleFS transition) cannot be done from this environment and must be performed by the user on a real drum unit after the plan is complete. The serial-monitor output of `mountFsWithMigration()` is the diagnostic surface for that test — keep its `Serial.println` calls intact.

## File Structure

Two files modified; no new files.

- `RX/platformio.ini` — `lib_deps` swap, add `board_build.filesystem = littlefs`
- `RX/src/main.cpp` — replace `#include <SPIFFSIniFile.h>`, add two static helpers (`readConfig`, `mountFsWithMigration`), replace the `#pragma region CONFIGFILE` block

## Task Ordering

Tasks 1–2 add new functions but do not call them — the firmware still builds with `SPIFFSIniFile` and SPIFFS active. Task 3 is the atomic switchover: it removes the old code path, adds the new call sites, and changes `platformio.ini` in a single commit because removing the `SPIFFSIniFile` dependency before the call sites are gone would break the build.

---

### Task 1: Add the INI parser helper

**Files:**
- Modify: `RX/src/main.cpp` (insert helper above `setup()`)

- [ ] **Step 1: Verify baseline build is clean**

Run:
```
cd RX && pio run -e esp12e
```
Expected: `===== [SUCCESS] Took ... =====`. If this fails, stop and report — the baseline is broken.

- [ ] **Step 2: Add `#include <LittleFS.h>` to the include block**

In `RX/src/main.cpp`, find the existing include block (around lines 12–19). Change the `#include "FS.h"` and `#include <SPIFFSIniFile.h>` pair to:

```cpp
#include <FS.h>
#include <LittleFS.h>
#include <SPIFFSIniFile.h>
```

Rationale: keep `SPIFFSIniFile.h` for now (still used by the existing CONFIGFILE block — removed in Task 3). Add `<LittleFS.h>` so the new helper compiles. Switch `"FS.h"` to `<FS.h>` for consistency with how the ESP8266 core ships it (cosmetic; both work).

- [ ] **Step 3: Add the `readConfig` static helper**

In `RX/src/main.cpp`, insert this function immediately after the `void (*resetFunc)(void) = 0;` line (around line 46) and before `showStatus`:

```cpp
// Read /config.ini from LittleFS. Returns true if the file opened
// successfully (regardless of which keys were present). Unknown keys
// and sections are silently ignored, so future config additions are
// non-breaking.
static bool readConfig(const char *path, int &numLeds, int &drumType)
{
  File f = LittleFS.open(path, "r");
  if (!f)
  {
    return false;
  }

  char section[16] = "";
  while (f.available())
  {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;
    if (line.startsWith(";") || line.startsWith("#"))
      continue;

    if (line.startsWith("["))
    {
      int end = line.indexOf(']');
      if (end > 1)
      {
        String s = line.substring(1, end);
        s.trim();
        s.toCharArray(section, sizeof(section));
      }
      continue;
    }

    int eq = line.indexOf('=');
    if (eq < 0)
      continue;
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim();
    val.trim();

    if (strcmp(section, "leds") == 0 && key == "count")
    {
      numLeds = val.toInt();
      Serial.print("Got numLeds from config: ");
      Serial.println(numLeds);
    }
    else if (strcmp(section, "drum") == 0 && key == "type")
    {
      drumType = val.toInt();
      Serial.print("Got drum type from config: ");
      Serial.println(drumType);
    }
  }
  f.close();
  return true;
}
```

- [ ] **Step 4: Build to verify**

Run:
```
cd RX && pio run -e esp12e
```
Expected: build SUCCESS. A `defined but not used` warning on `readConfig` is acceptable at this stage — it gets called in Task 3.

- [ ] **Step 5: Commit**

```
git add RX/src/main.cpp
git commit -m "feat(rx): add custom INI parser for LittleFS config"
```

---

### Task 2: Add the SPIFFS → LittleFS migration helper

**Files:**
- Modify: `RX/src/main.cpp` (insert second helper above `setup()`)

- [ ] **Step 1: Add `mountFsWithMigration` below `readConfig`**

In `RX/src/main.cpp`, insert this function immediately after the `readConfig` function added in Task 1:

```cpp
// One-time migration: if LittleFS isn't mounted (the flash region is
// still SPIFFS-formatted), read /config.ini from SPIFFS into RAM,
// format LittleFS, and write the file back. Idempotent on subsequent
// boots — LittleFS.begin() succeeds first try and the SPIFFS branch
// never runs. Returns true if LittleFS is mounted on exit.
static bool mountFsWithMigration(const char *configPath)
{
  if (LittleFS.begin())
  {
    return true;
  }

  Serial.println("LittleFS not present; attempting SPIFFS migration");

  const size_t BUF_SZ = 1024;
  char buffer[BUF_SZ];
  size_t bufLen = 0;
  bool haveConfig = false;

  if (SPIFFS.begin())
  {
    if (SPIFFS.exists(configPath))
    {
      File f = SPIFFS.open(configPath, "r");
      if (f)
      {
        bufLen = f.readBytes(buffer, BUF_SZ);
        if (f.available())
        {
          Serial.println("WARN: config.ini larger than 1024 bytes; trailing data lost");
        }
        f.close();
        haveConfig = bufLen > 0;
        Serial.printf("Read %u bytes of config from SPIFFS\n", (unsigned)bufLen);
      }
    }
    else
    {
      Serial.println("No /config.ini on SPIFFS; LittleFS will start empty");
    }
    SPIFFS.end();
  }
  else
  {
    Serial.println("SPIFFS also unmountable; formatting LittleFS fresh");
  }

  if (!LittleFS.format())
  {
    Serial.println("LittleFS.format() failed");
    return false;
  }
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS.begin() after format failed");
    return false;
  }

  if (haveConfig)
  {
    File out = LittleFS.open(configPath, "w");
    if (!out)
    {
      Serial.println("Could not open /config.ini for write on LittleFS");
      return true;
    }
    out.write((const uint8_t *)buffer, bufLen);
    out.close();
    Serial.println("Wrote config.ini to LittleFS");
  }

  return true;
}
```

- [ ] **Step 2: Build to verify**

Run:
```
cd RX && pio run -e esp12e
```
Expected: build SUCCESS. `defined but not used` warnings on both helpers are still acceptable.

- [ ] **Step 3: Commit**

```
git add RX/src/main.cpp
git commit -m "feat(rx): add one-shot SPIFFS to LittleFS migration helper"
```

---

### Task 3: Switch to LittleFS at the call sites and update `platformio.ini`

This task is atomic — call-site change, include cleanup, and dependency removal in one commit, because each piece breaks the build without the others.

**Files:**
- Modify: `RX/src/main.cpp` (CONFIGFILE region in `setup()`, plus the `#include <SPIFFSIniFile.h>` line)
- Modify: `RX/platformio.ini` (lib_deps, board_build.filesystem)

- [ ] **Step 1: Remove the `SPIFFSIniFile` include**

In `RX/src/main.cpp`, delete the line:

```cpp
#include <SPIFFSIniFile.h>
```

The include block should now end with `#include <LittleFS.h>` followed by `#include "prototypes.h"`.

- [ ] **Step 2: Replace the CONFIGFILE block in `setup()`**

In `RX/src/main.cpp`, find the existing block between `#pragma region CONFIGFILE` and the `ini.close();` line (currently lines 83–130 inclusive). Replace the **entire** block — from `#pragma region CONFIGFILE` through `ini.close();` — with:

```cpp
#pragma region CONFIGFILE

  const char *filename = "/config.ini";

  if (!mountFsWithMigration(filename))
  {
    Serial.println("Filesystem mount failed");
    ledMode = -2;
  }
  else
  {
    int drumType = 0;
    if (!readConfig(filename, numLeds, drumType))
    {
      Serial.print("Config file ");
      Serial.print(filename);
      Serial.println(" not found; using defaults");
    }
  }

#pragma endregion CONFIGFILE
```

Notes:
- The local `buffer[bufferLen]` from the old block is gone — the new parser owns its own state internally.
- `drumType` is still declared but its value is only used for the log line emitted by `readConfig`; this matches existing behavior (the original code reads it but never uses it after).
- The "got numLeds" / "got drum type" log lines that previously lived in `setup()` now live inside `readConfig` (so they only print when the key is actually present, matching the old `ini.getValue()` semantics).

- [ ] **Step 3: Update `RX/platformio.ini`**

Replace the file contents with:

```ini
; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html

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

Changes from the existing file: added the `board_build.filesystem = littlefs` line; removed `yurilopes/SPIFFSIniFile@^1.0.0` from `lib_deps`.

- [ ] **Step 4: Build to verify**

Run:
```
cd RX && pio run -e esp12e
```
Expected: build SUCCESS, with no `defined but not used` warnings (both helpers are now called from `setup()`).

If the build fails complaining about `SPIFFSIniFile.h not found` after `pio` re-resolves deps, run:

```
cd RX && pio run -e esp12e -t clean && pio run -e esp12e
```

If it fails complaining about `LittleFS.h`, the ESP8266 core is too old. Check `pio platform show espressif8266 | grep version` — needs core 3.0.0+. Report this to the user rather than attempting a fix.

- [ ] **Step 5: Commit**

```
git add RX/src/main.cpp RX/platformio.ini
git commit -m "feat(rx): switch from SPIFFS to LittleFS with first-boot migration"
```

---

### Task 4: Final verification and handoff notes

- [ ] **Step 1: Confirm the working tree is clean and the build is still green**

Run:
```
git status
cd RX && pio run -e esp12e
```
Expected: `git status` shows no uncommitted changes (other than build artifacts); build succeeds.

- [ ] **Step 2: Inspect the final `main.cpp` against the spec**

Open `RX/src/main.cpp` and confirm:
- No remaining references to `SPIFFSIniFile`, `SPIFFSIniFile.h`, or `ini.` (the old IniFile object).
- `SPIFFS` is referenced **only** inside `mountFsWithMigration` (the one-shot migration path).
- `LittleFS` is referenced in `readConfig` and `mountFsWithMigration`.
- The error path still sets `ledMode = -2` on filesystem failure (preserves the existing DarkMagenta error indicator).

Run a quick grep to verify:
```
grep -nE 'SPIFFS|LittleFS|ini\.' RX/src/main.cpp
```

Expected matches:
- `SPIFFS` only inside `mountFsWithMigration`
- `LittleFS` in includes, `readConfig`, and `mountFsWithMigration`
- No `ini.` matches

- [ ] **Step 3: Write a brief handoff summary for the user**

Report to the user, in the final response:

1. Branch state: commits added on `claude/jolly-dijkstra-fbda9d`.
2. Build: `pio run -e esp12e` passes from `RX/`.
3. **What the user must verify on hardware (cannot be done from here):**
   - Flash one drum that has an existing SPIFFS `/config.ini`. On first boot, the serial monitor should print `LittleFS not present; attempting SPIFFS migration` followed by `Read N bytes of config from SPIFFS` and `Wrote config.ini to LittleFS`. The drum should then come up with its previous `numLeds` / `drumType` values logged.
   - Reset the same drum. On second boot, the migration messages should be absent — `LittleFS.begin()` succeeds on the first try, and only the `Got numLeds from config` / `Got drum type from config` lines appear.
   - (Optional) Flash a blank ESP12E. It should fall through to defaults (no `Got numLeds` line) and run without an error LED.
4. The SPIFFS migration code is intentionally retained as a safety net. Once all deployed drums have booted on this firmware at least once, a follow-up change can strip `SPIFFS.begin()` / `SPIFFS.open()` / etc. from `mountFsWithMigration`.

---

## Self-Review

Spec coverage check (each spec section → task):
- platformio.ini changes → Task 3, Step 3 ✓
- First-boot migration flow → Task 2 (helper) + Task 3 (call site) ✓
- 1024-byte buffer with overflow warning → Task 2, Step 1 (`BUF_SZ = 1024`, `if (f.available())` warning) ✓
- Tiny custom INI parser (signature: `readConfig(const char*, int&, int&)`) → Task 1, Step 3 ✓
- Unknown keys silently ignored → Task 1, Step 3 (no else branch) ✓
- Defaults when file missing → Task 3, Step 2 (only the "not found" log; `numLeds` / `drumType` keep their initial values) ✓
- `ledMode = -2` on filesystem failure → Task 3, Step 2 ✓
- Files touched: `RX/platformio.ini`, `RX/src/main.cpp` only → Tasks 1–3 ✓
- Verification: `pio run -e esp12e` builds → Task 4, Step 1 ✓
- Hardware verification documented for user → Task 4, Step 3 ✓

No spec section is unaccounted for. No placeholder phrases. Function signatures consistent across tasks (`readConfig`, `mountFsWithMigration`, `numLeds`, `drumType`).
