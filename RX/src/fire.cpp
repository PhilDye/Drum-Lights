#include <FastLED.h>

#define MAX_LEDS 104 // Maximum number of LEDS to initialise for

void fire(struct CRGB *targetArray, int numLeds, const struct CRGBPalette16 &colorPalette)
{
// COOLING: How quickly does each cell cool down?
// Less cooling = longer-lived flames.  More cooling = shorter-lived flames.
// suggested range 20-100
#define COOLING 100

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more intense fire.  Lower chance = more flickery fire.
// suggested range 50-200.
#define SPARKING 100

    // Temperature readings at each simulation cell
    static uint8_t heat[MAX_LEDS];

    // Step 1.  Cool down every cell a little
    for (byte i = 0; i < numLeds; i++)
    {
        heat[i] = qsub8(heat[i], random8(0, (COOLING / numLeds)));
    }

    // Step 2.  Heat from each cell drifts 'outwards'
    for (byte k = 1; k < numLeds; k++) // skip pixel 0 to avoid -1 error
    {
        heat[k - 1] = (heat[k] + heat[k - 1]) / 2;
        heat[k + 1] = (heat[k] + heat[k + 1]) / 2;
        heat[k] = heat[k] / 1.1;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat somewhere
    if (random8() < SPARKING)
    {
        byte sparkHeat = random8(160, 255);
        byte y = random8(numLeds); // where to spark
        heat[y] = qadd8(heat[y], sparkHeat);
    }

    // Step 4.  Map from heat cells to LED colors
    for (byte j = 0; j < numLeds; j++)
    {
        // Scale the heat value from 0-255 down to 0-240
        // for best results with color palettes.
        byte colorindex = scale8(heat[j], 240);
        CRGB color = ColorFromPalette(colorPalette, colorindex);

        targetArray[j] = color;
    }

    // FastLED.delay(250);
}
