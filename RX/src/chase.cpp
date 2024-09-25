#include <FastLED.h>

void chase(struct CRGB *targetArray, int numLeds, const struct CRGB &color)
{

  static uint8_t i = 0;
  const uint8_t SEGMENTS = 4;

  static uint8_t segmentSize = (uint8_t)numLeds / SEGMENTS;

  fadeToBlackBy(targetArray, numLeds, 100);

  for (uint8_t s = 0; s < SEGMENTS; s++)
  {
    uint8_t target = i + (segmentSize * s);
    while (target > numLeds)
    {
      Serial.println("Busted target!! " + target);
      target = numLeds - 1;
    }
    targetArray[target] = color;
  }

  i++;
  // restart once we reach the end of each segment
  if (i == segmentSize)
    i = 0;
}
