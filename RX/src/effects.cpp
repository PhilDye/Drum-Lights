#include <FastLED.h>

#define MAX_LEDS 104 // Maximum number of LEDS to initialise for

void rioSpin(struct CRGB *targetArray, int numLeds)
{
    const uint8_t SEGMENTS = 3;
    uint8_t stripeLength = numLeds / SEGMENTS; // number of pixels per color
    static int offset = 0;

    for (uint8_t i = 0; i < numLeds; i++)
    {
        int target = i + offset;

        if (target > numLeds - 1)
        {
            target = target % numLeds;
        }

        if (i < stripeLength)
        {
            targetArray[target] = CRGB::Green;
        }
        else if (i < 2 * stripeLength)
        {
            targetArray[target] = CRGB::Gold;
        }
        else
        {
            targetArray[target] = CRGB::DarkBlue;
        }
    }

    offset++;
    if (offset == numLeds)
    {
        offset = 0;
    }
}

void rioDisco(CRGB leds[], int numLeds)
{
    static uint8_t activePixels = numLeds / 20; // controls density of lit pixels
    static uint16_t lastPixel = 0;

    for (uint8_t i = 0; i < activePixels; i++)
    {
        fadeToBlackBy(leds, numLeds, numLeds / activePixels);

        uint8_t c = random8(0, 3);
        switch (c)
        {
        case 0:
            leds[lastPixel] = CRGB::Green;
            break;
        case 1:
            leds[lastPixel] = CRGB::Gold;
            break;
        case 2:
            leds[lastPixel] = CRGB::DarkBlue;
            break;
        }

        lastPixel = random8(numLeds - 1);

        while (leds[lastPixel].red > 0 || leds[lastPixel].green > 0 || leds[lastPixel].blue > 0)
        {
            // pixel already lit, pick again!
            lastPixel = random8(numLeds - 1);
        }
        // leds[lastPixel] = CRGB::White;
    }
    // FastLED.delay(10);     // slow things down
}

void rioFlag(struct CRGB *targetArray, int numLeds)
{

    static int offset = 0;

    for (uint8_t i = 0; i < numLeds; i++)
    {
        int target = i + offset;

        if (target > numLeds - 1)
        {
            target = target % numLeds;
        }

        switch (i)
        {
        case 0:
        case 1:
        case 2:
            targetArray[target] = CRGB::Green;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            targetArray[target] = CRGB::Gold;
            break;
        case 7:
        case 8:
            targetArray[target] = CRGB::DarkBlue;
            break;
        case 9:
            targetArray[target] = CRGB::White;
            break;
        case 10:
        case 11:
            targetArray[target] = CRGB::DarkBlue;
            break;
        case 12:
        case 13:
        case 14:
        case 15:
            targetArray[target] = CRGB::Gold;
            break;
        case 16:
        case 17:
        case 18:
            targetArray[target] = CRGB::Green;
            break;
        default:
            targetArray[target] = CRGB::Black;
            break;
        }
    }

    offset++;
    if (offset == numLeds)
    {
        offset = 0;
    }
}

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

void colorTwinkle(struct CRGB *targetArray, int numLeds, const struct CRGB &color0, const struct CRGB &color1, const struct CRGB &color2)
{
    static byte activePixels = numLeds / 20; // controls density of lit pixels
    static byte lastPixel = 0;

    for (byte i = 0; i < activePixels; i++)
    {
        fadeToBlackBy(targetArray, numLeds, numLeds / activePixels);

        byte color = random(0,3);

        if (color0.getLuma() > 0 && color == 0)
        {
            targetArray[lastPixel] = color0;
        }
        else if(color1.getLuma() > 0 && color == 1)
        {
            targetArray[lastPixel] = color1;
        }
        else if(color2.getLuma() > 0 && color == 2)
        {
            targetArray[lastPixel] = color2;
        }
        else //fail safe
        {
            targetArray[lastPixel] = color0;
        }

        lastPixel = random8(numLeds - 1);

        while (targetArray[lastPixel].red > 0 || targetArray[lastPixel].green > 0 || targetArray[lastPixel].blue > 0)
        {
            // pixel already lit, pick again!
            lastPixel = random8(numLeds - 1);
        }
    }
}

void rainbow(CRGB leds[], int numLeds)
{
    uint8_t thisHue = beat8(60, 255);
    fill_rainbow(targetArray, numLeds, thisHue, 7);
}

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