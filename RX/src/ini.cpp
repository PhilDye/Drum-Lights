#include "ini.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool isCommentChar(char c)
{
  return (c == ';' || c == '#');
}

// Advance past the end of the current line, consuming one "\n", "\r",
// "\r\n" or "\n\r" terminator. Returns the length of the line.
static size_t nextLine(const char *text, size_t len, size_t &pos)
{
  size_t start = pos;
  while (pos < len && text[pos] != '\n' && text[pos] != '\r')
    ++pos;
  size_t lineLen = pos - start;
  if (pos < len)
  {
    char other = (text[pos] == '\n' ? '\r' : '\n');
    ++pos;
    if (pos < len && text[pos] == other)
      ++pos;
  }
  return lineLen;
}

// Trim whitespace from both ends of [start, start+len) in place
static void trim(const char *&start, size_t &len)
{
  while (len > 0 && isspace((unsigned char)*start))
  {
    ++start;
    --len;
  }
  while (len > 0 && isspace((unsigned char)start[len - 1]))
    --len;
}

static bool nameEquals(const char *s, size_t len, const char *name)
{
  return strlen(name) == len && strncasecmp(s, name, len) == 0;
}

bool IniConfig::load(const char *text, size_t len)
{
  if (len > INI_MAX_SIZE)
    return false;
  memcpy(_text, text, len);
  _text[len] = '\0';
  _len = len;
  return true;
}

bool IniConfig::validate() const
{
  size_t pos = 0;
  while (pos < _len)
  {
    // SPIFFSIniFile read 80-byte chunks, so it also accepted a longer line
    // when it was within the last 80 bytes of the file. Match it so existing
    // files validate the same way.
    size_t remaining = _len - pos;
    if (nextLine(_text, _len, pos) > INI_MAX_LINE_LEN && remaining > INI_MAX_LINE_LEN + 2)
      return false;
  }
  return true;
}

bool IniConfig::getValue(const char *section, const char *key, char *value, size_t valueLen) const
{
  if (section == NULL || key == NULL || *key == '\0' || valueLen == 0)
    return false;

  bool inSection = false;
  size_t pos = 0;
  while (pos < _len)
  {
    const char *line = _text + pos;
    size_t lineLen = nextLine(_text, _len, pos);
    trim(line, lineLen);

    if (lineLen == 0 || isCommentChar(*line))
      continue;

    if (!inSection)
    {
      if (*line != '[')
        continue;
      const char *end = (const char *)memchr(line, ']', lineLen);
      if (end == NULL)
        continue;
      const char *name = line + 1;
      size_t nameLen = end - name;
      trim(name, nameLen);
      inSection = nameEquals(name, nameLen, section);
      continue;
    }

    // Reached the next section without finding the key
    if (*line == '[')
      return false;

    const char *eq = (const char *)memchr(line, '=', lineLen);
    if (eq == NULL)
      continue;
    const char *name = line;
    size_t nameLen = eq - line;
    trim(name, nameLen);
    if (!nameEquals(name, nameLen, key))
      continue;

    const char *val = eq + 1;
    size_t len = lineLen - (val - line);
    trim(val, len);
    if (len >= valueLen)
      return false;
    memcpy(value, val, len);
    value[len] = '\0';
    return true;
  }
  return false;
}

bool IniConfig::getValue(const char *section, const char *key, int &value) const
{
  char buffer[INI_MAX_LINE_LEN + 1];
  if (!getValue(section, key, buffer, sizeof(buffer)))
    return false;
  value = atoi(buffer);
  return true;
}
