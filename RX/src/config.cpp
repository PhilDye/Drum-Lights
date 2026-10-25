#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"

static char fileBuffer[INI_MAX_SIZE + 1];

static bool readFile(fs::FS &fs, const char *filename, size_t &len)
{
  File file = fs.open(filename, "r");
  if (!file)
  {
    Serial.printf("ini file %s does not exist\n", filename);
    return false;
  }
  if (file.size() > INI_MAX_SIZE)
  {
    Serial.printf("ini file %s is too large (%u bytes)\n", filename, (unsigned)file.size());
    file.close();
    return false;
  }
  len = file.read((uint8_t *)fileBuffer, file.size());
  bool ok = (len == file.size());
  file.close();
  return ok;
}

// SPIFFS is deprecated; it is used only to read the config from boards that
// were set up before the move to LittleFS. This can be removed once every
// board has booted this firmware at least once.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static bool readLegacySpiffs(const char *filename, size_t &len)
{
  // Never let SPIFFS format the partition if it fails to mount
  SPIFFS.setConfig(SPIFFSConfig(false));
  if (!SPIFFS.begin())
  {
    Serial.println("No SPIFFS filesystem found either");
    return false;
  }
  bool ok = readFile(SPIFFS, filename, len);
  SPIFFS.end();
  return ok;
}
#pragma GCC diagnostic pop

static bool migrateToLittleFS(const char *filename, size_t len)
{
  Serial.println("Migrating config from SPIFFS to LittleFS. Contents:");
  Serial.write((const uint8_t *)fileBuffer, len);
  Serial.println();

  if (!LittleFS.format() || !LittleFS.begin())
  {
    Serial.println("LittleFS format failed");
    return false;
  }

  File file = LittleFS.open(filename, "w");
  if (!file)
  {
    Serial.println("Could not create config on LittleFS");
    return false;
  }
  size_t written = file.write((const uint8_t *)fileBuffer, len);
  file.close();

  // Read back to confirm the copy is intact
  file = LittleFS.open(filename, "r");
  bool ok = (written == len && file && file.size() == len);
  for (size_t i = 0; ok && i < len; i++)
    ok = (file.read() == (uint8_t)fileBuffer[i]);
  file.close();

  Serial.println(ok ? "Migration complete" : "Migration failed verification");
  return ok;
}

bool loadConfig(const char *filename, IniConfig &ini)
{
  size_t len = 0;
  bool legacy = false;

  // Never auto-format: an unformatted partition may hold a legacy SPIFFS
  // filesystem with this board's config on it
  LittleFS.setConfig(LittleFSConfig(false));

  if (LittleFS.begin())
  {
    if (!readFile(LittleFS, filename, len))
      return false;
  }
  else
  {
    Serial.println("LittleFS mount failed, trying legacy SPIFFS");
    if (!readLegacySpiffs(filename, len))
      return false;
    legacy = true;
  }

  if (!ini.load(fileBuffer, len))
    return false;

  if (!ini.validate())
  {
    Serial.printf("ini file %s not valid: line longer than %d characters\n", filename, INI_MAX_LINE_LEN);
    if (legacy)
      Serial.println("Leaving SPIFFS untouched");
    return false;
  }

  // Only reformat once we hold a valid copy of the config in RAM. If the
  // migration fails, this boot still uses the copy already loaded.
  if (legacy)
    migrateToLittleFS(filename, len);

  return true;
}
