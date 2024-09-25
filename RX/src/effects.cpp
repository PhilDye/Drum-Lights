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

void rainbow(struct CRGB *targetArray, int numLeds)
{
    uint8_t thisHue = beat8(60, 255);
    fill_rainbow(targetArray, numLeds, thisHue, 7);
}
