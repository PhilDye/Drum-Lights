#include <FastLED.h>

void chase(struct CRGB *targetArray, int numLeds, const struct CRGB &color0, const struct CRGB &color1 = CRGB::Black)
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

    if (color1.getLuma() > 0)
    {
      switch (s)
      {
        case 0:
        case 2:
          targetArray[target] = color0;
          break;

        case 1:
        case 3:
          targetArray[target] = color1;
          break;

        default:    // fail-safe
          targetArray[target] = color0;
          break;
      }
    } else {
      targetArray[target] = color0;
    }
  }

  i++;
  // restart once we reach the end of each segment
  if (i == segmentSize)
    i = 0;
  
}
