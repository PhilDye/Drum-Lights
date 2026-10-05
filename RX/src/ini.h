#pragma once

#include <stddef.h>

#define INI_MAX_SIZE 1024    // Largest config file we will load
#define INI_MAX_LINE_LEN 78  // Longest line accepted by validate()

// Minimal in-memory INI reader. Parsing rules match the SPIFFSIniFile library
// this replaces, so existing config.ini files read identically:
// - section and key names are case-insensitive
// - lines starting with ';' or '#' (after whitespace) are comments
// - whitespace around section names, keys and values is ignored
// - the first matching section and key wins
// - integer values are converted with atoi()
class IniConfig
{
public:
  // Copy the file contents into the internal buffer
  bool load(const char *text, size_t len);

  // False if any line is longer than INI_MAX_LINE_LEN characters
  bool validate() const;

  bool getValue(const char *section, const char *key, char *value, size_t valueLen) const;
  bool getValue(const char *section, const char *key, int &value) const;

private:
  char _text[INI_MAX_SIZE + 1] = {0};
  size_t _len = 0;
};
