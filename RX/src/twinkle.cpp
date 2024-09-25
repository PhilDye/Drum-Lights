#include <FastLED.h>

#define MAX_LEDS 104 // Maximum number of LEDS to initialise for

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

void rioDisco(struct CRGB *targetArray, int numLeds)
{
    colorTwinkle(targetArray, numLeds, CRGB::Green, CRGB::Gold, CRGB::DarkBlue);
}
