#include <FastLED.h>

void chase(CRGB color, CRGB leds[],  int numLeds)
{
  static uint8_t i = 0;
  const uint8_t SEGMENTS = 4;

  static uint8_t segmentSize = (uint8_t)numLeds / SEGMENTS;

  fadeToBlackBy(leds, numLeds, 100);

  for (uint8_t s = 0; s < SEGMENTS; s++)
  {
    uint8_t target = i + (segmentSize * s);
    if (target > numLeds)
    {
      Serial.println("Busted target!! " + target);
      target = numLeds - 1;
    }
    leds[target] = color;
  }

  i++;
  // restart once we reach the end of each segment
  if (i == segmentSize)
    i = 0;
}
