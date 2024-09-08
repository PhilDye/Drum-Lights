#include <FastLED.h>

void rioSpin(CRGB leds[], int numLeds)
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
            leds[target] = CRGB::Green;
        }
        else if (i < 2 * stripeLength)
        {
            leds[target] = CRGB::Gold;
        }
        else
        {
            leds[target] = CRGB::DarkBlue;
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

void rioFlag(CRGB leds[], int numLeds)
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
            leds[target] = CRGB::Green;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            leds[target] = CRGB::Gold;
            break;
        case 7:
        case 8:
            leds[target] = CRGB::DarkBlue;
            break;
        case 9:
            leds[target] = CRGB::White;
            break;
        case 10:
        case 11:
            leds[target] = CRGB::DarkBlue;
            break;
        case 12:
        case 13:
        case 14:
        case 15:
            leds[target] = CRGB::Gold;
            break;
        case 16:
        case 17:
        case 18:
            leds[target] = CRGB::Green;
            break;
        default:
            leds[target] = CRGB::Black;
            break;
        }
    }

    offset++;
    if (offset == numLeds)
    {
        offset = 0;
    }
}


}

void colorTwinkle(struct CRGB *targetArray, int numLeds, const struct CRGB &color)
{
    static byte activePixels = numLeds / 20; // controls density of lit pixels
    static byte lastPixel = 0;

    for (byte i = 0; i < activePixels; i++)
    {
        fadeToBlackBy(targetArray, numLeds, numLeds / activePixels);

        targetArray[lastPixel] = color;

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
    fill_rainbow(leds, numLeds, thisHue, 7);
}

void hazards(CRGB leds[], int numLeds)
{
    size_t quartiles = numLeds / 4;
    const size_t size = 4;

    EVERY_N_MILLISECONDS(1000)
    {
        for (size_t i = quartiles - size / 2; i < quartiles + size / 2; i++)
        {
            leds[i] = CRGB::DarkOrange;
        }
        for (size_t i = 3 * quartiles - size / 2; i < 3 * quartiles + size / 2; i++)
        {
            leds[i] = CRGB::DarkOrange;
        }
        FastLED.show();
        FastLED.delay(500);
        FastLED.clear(true);
    }
}

void nineninenine(CRGB leds[], int numLeds)
{
    size_t quarter = numLeds / 4;

    const int flashes = 3;

    for (size_t f = 0; f < flashes; f++)
    {
        // quadrant 1
        for (size_t i = 0; i < quarter; i++)
        {
            leds[i] = CRGB::Blue;
        }
        // quadrant 3
        for (size_t i = 2 * quarter; i < 3 * quarter; i++)
        {
            leds[i] = CRGB::Blue;
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
            leds[i] = CRGB::Blue;
        }
        // quadrant 4
        for (size_t i = 3 * quarter; i < numLeds; i++)
        {
            leds[i] = CRGB::Blue;
        }
        FastLED.show();
        FastLED.delay(25);
        FastLED.clear(true);
        FastLED.delay(25);
    }
    FastLED.delay(200);
}