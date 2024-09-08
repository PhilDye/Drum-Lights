#include <FastLED.h>

// 1-7  // solid green, yellow, blue, red, white, cyan, magenta

// 11-17    // green, yellow, blue, red, white, cyan, magenta
void chase(struct CRGB *targetArray, int numLeds, const struct CRGB &color);

// 81-87
void colorTwinkle(struct CRGB *targetArray, int numLeds, const struct CRGB &color);

// 91-93
void rioSpin(CRGB targetArray[], int numLeds);
void rioDisco(CRGB targetArray[], int numLeds);
void rioFlag(CRGB targetArray[], int numLeds);

// 97
void hazards(CRGB targetArray[], int numLeds);

// 98
// strobe

// 99
void rainbow(CRGB targetArray[], int numLeds);

// 199 blue strobe
void nineninenine(CRGB targetArray[], int numLeds);
