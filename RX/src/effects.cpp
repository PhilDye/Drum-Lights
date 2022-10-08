#include <FastLED.h>

void rioSpin(CRGB leds[], int numLeds)
{
    const uint8_t SEGMENTS = 3;
    uint8_t stripeLength = numLeds / SEGMENTS; // number of pixels per color
    static int offset = 0;

    for (uint8_t i = 0; i < numLeds; i++)
    {
        int target = i + offset;

        if (target > numLeds-1)
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
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy( random(65536));

    static uint8_t activePixels = numLeds / 20;     // controls density of lit pixels
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

        lastPixel = random8(numLeds-1);

        while (leds[lastPixel].red > 0 || leds[lastPixel].green > 0 || leds[lastPixel].blue > 0)
        {
            //pixel already lit, pick again!
            lastPixel = random8(numLeds-1);
        }
        //leds[lastPixel] = CRGB::White;
    }
    FastLED.delay(10);     // slow things down

}

void rioFlag(CRGB leds[], int numLeds)
{

    static int offset = 0;

    for (uint8_t i = 0; i < numLeds; i++)
    {
        int target = i + offset;

        if (target > numLeds-1)
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

void colorTwinkle(CRGB color, CRGB leds[], int numLeds)
{
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy( random(65536));

    static uint8_t activePixels = numLeds / 20;     // controls density of lit pixels
    static uint16_t lastPixel = 0;

    for (uint8_t i = 0; i < activePixels; i++)
    {    
        fadeToBlackBy(leds, numLeds, numLeds / activePixels);

        leds[lastPixel] = color;

        lastPixel = random8(numLeds-1);

        while (leds[lastPixel].red > 0 || leds[lastPixel].green > 0 || leds[lastPixel].blue > 0)
        {
            //pixel already lit, pick again!
            lastPixel = random8(numLeds-1);
        }
        //leds[lastPixel] = CRGB::White;
    }
    FastLED.delay(10);     // slow things down

}

void rainbow(CRGB leds[], int numLeds)
{
    uint8_t thisHue = beat8(60,255);
    fill_rainbow( leds, numLeds, thisHue, 7);
}