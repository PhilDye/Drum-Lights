#pragma once

#include "ini.h"

// Mount LittleFS and load the config file into ini. Boards flashed before the
// move to LittleFS still have a SPIFFS filesystem; on their first boot the
// config file is read from SPIFFS, the partition is reformatted as LittleFS
// and the file is written back unchanged.
// Returns false if the filesystem or file could not be read, or is invalid.
bool loadConfig(const char *filename, IniConfig &ini);
