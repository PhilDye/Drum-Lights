#include <FastLED.h>

#define MAX_LEDS 104 // Maximum number of LEDS to initialise for

void hazards(struct CRGB *targetArray, int numLeds)
{
    size_t quartiles = numLeds / 4;
    const size_t size = 4;

    EVERY_N_MILLISECONDS(1000)
    {
        for (size_t i = quartiles - size / 2; i < quartiles + size / 2; i++)
        {
            targetArray[i] = CRGB::DarkOrange;
        }
        for (size_t i = 3 * quartiles - size / 2; i < 3 * quartiles + size / 2; i++)
        {
            targetArray[i] = CRGB::DarkOrange;
        }
        FastLED.show();
        FastLED.delay(500);
        FastLED.clear(true);
    }
}

void nineninenine(struct CRGB *targetArray, int numLeds)
{
    size_t quarter = numLeds / 4;

    const int flashes = 3;

    for (size_t f = 0; f < flashes; f++)
    {
        // quadrant 1
        for (size_t i = 0; i < quarter; i++)
        {
            targetArray[i] = CRGB::Blue;
        }
        // quadrant 3
        for (size_t i = 2 * quarter; i < 3 * quarter; i++)
        {
            targetArray[i] = CRGB::Blue;
        }
        FastLED.show();
        FastLED.delay(25);
        FastLED.clear(true);
        FastLED.delay(25);
    }
    FastLED.delay(200);

    for (size_t f = 0; f < flashes; f++)
    {
        // quadrant 2
        for (size_t i = quarter; i < 2 * quarter; i++)
        {
            targetArray[i] = CRGB::Blue;
        }
        // quadrant 4
        for (size_t i = 3 * quarter; i < numLeds; i++)
        {
            targetArray[i] = CRGB::Blue;
        }
        FastLED.show();
        FastLED.delay(25);
        FastLED.clear(true);
        FastLED.delay(25);
    }
    FastLED.delay(200);
}
