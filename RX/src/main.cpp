/* Drum Lights receiver
 *
 * by Phil Dye <phil@phildye.org>
 * for Swan Samba Band <https://www.swansamba.co.uk>
 *
 * Works with the Drum Lights Transmitter
 * to receive desired colour/pattern commands from a web UI
 * by nRF24L01 radio module, and outputs to a WS2812B LED strip
 *
 */

#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include "FS.h"
#include <SPIFFSIniFile.h>

#include "prototypes.h"

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

// Setup the nRF24L01 radio
#define NRF24L01_PIN_CE 4
#define NRF24L01_PIN_CS 15
struct RF24 radio(NRF24L01_PIN_CE, NRF24L01_PIN_CS);
const byte address[5] = {'R', 'x', 'A', 'A', '1'};

// Setup the LEDs
#define MAX_LEDS 104 // Maximum number of LEDS to initialise for
#define DATA_PIN 2
#define VOLTS 5
#define MAX_MA 2000
#define FRAMES_PER_SECOND 35
byte max_bright = 255;      // Overall brightness definition, could be changed on the fly
struct CRGB leds[MAX_LEDS]; // The array of leds, one for each led in the strip
int numLeds = MAX_LEDS;    // To be read from config later

byte ledMode = 0; // The currently active pattern

void (*resetFunc)(void) = 0; // declare reset function @ address 0

void showStatus(struct CRGB *targetArray, const struct CRGB &color)
{
  EVERY_N_MILLIS(1000)
  {
    targetArray[1] = color;
    FastLED.show();
    FastLED.delay(500);
    FastLED.clear(true);
    FastLED.delay(500);
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    // some boards need to wait to ensure access to serial over USB
  }
  delay(1000);

  Serial.println("Setting up...");

  // to read config file
  const byte bufferLen = 80;
  char buffer[bufferLen];

  const char *filename = "/config.ini";

  // Mount the SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS.begin() failed");
    showStatus(leds, CRGB::Cyan);
    ledMode = 253;
  }

  SPIFFSIniFile ini(filename);
  if (!ini.open())
  {
    Serial.print("ini file ");
    Serial.print(filename);
    Serial.println(" does not exist");
    ledMode = 253;
  }

  // Check the file is valid. This can be used to warn if any lines
  // are longer than the buffer.
  if (!ini.validate(buffer, bufferLen))
  {
    Serial.print("ini file ");
    Serial.print(ini.getFilename());
    Serial.print(" not valid: ");
    ledMode = 253;
  }

  if (ini.getValue("leds", "count", buffer, bufferLen, numLeds))
  {
    Serial.print("Got numLeds from config: ");
    Serial.println(numLeds);
  }
  long drumType = 0;
  if (ini.getValue("drum", "type", buffer, bufferLen, drumType))
  {
    Serial.print("Got drum type from config: ");
    Serial.println(drumType);
  }

  Serial.print("Setting up LEDs... ");
  LEDS.addLeds<WS2812, DATA_PIN, GRB>(leds, numLeds).setCorrection(TypicalPixelString);
  ;
  FastLED.setBrightness(max_bright);
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
  FastLED.clear();
  Serial.println("Done");

  Serial.printf("ESP8266 Chip id = %08X\n", ESP.getChipId());

  Serial.print("Setting up radio... ");
  if (radio.begin())
  {
    radio.openReadingPipe(1, address);
    radio.setAutoAck(false);
    radio.startListening();     // put radio in TX mode
    radio.printPrettyDetails(); // (larger) function that prints human readable data
    Serial.print("done");

    ledMode = 53;
  }
  else
  {
    Serial.println(F("radio hardware is not responding!!"));
    ledMode = 254;
  }
}

void readRadio()
{
  byte pipe;

  if (radio.available(&pipe))
  { // is there a payload?
    byte payLoadSize = radio.getPayloadSize();
    radio.read(&ledMode, payLoadSize); // fetch payload from FIFO
    Serial.print("Radio RX data: ");
    Serial.println(ledMode);

    // if (ledMode > 47)
    // {
    //   ledMode = ledMode - 48; // seems to be an offset?!
    // }

    // clear all pixels ready for the new mode
    FastLED.clear();
  }
}

void loop()
{
  // Add entropy to random number generator; we use a lot of it
  random16_add_entropy(random());

  byte currentMode = ledMode;

  readRadio();

  switch (ledMode)
  {
  case 0:
    showStatus(leds, CRGB::DarkGreen);
    break;

  case 1:
    fill_solid(leds, numLeds, CRGB::Green);
    break;
  case 2:
    fill_solid(leds, numLeds, CRGB::Gold);
    break;
  case 3:
    fill_solid(leds, numLeds, CRGB::DarkBlue);
    break;
  case 4:
    fill_solid(leds, numLeds, CRGB::Red);
    break;
  case 5:
    fill_solid(leds, numLeds, CRGB::White);
    break;
  case 6:
    fill_solid(leds, numLeds, CRGB::Cyan);
    break;
  case 7:
    fill_solid(leds, numLeds, CRGB::Purple);
    break;

  case 11:
    chase(leds, numLeds, CRGB::Green);
    break;
  case 12:
    chase(leds, numLeds, CRGB::Gold);
    break;
  case 13:
    chase(leds, numLeds, CRGB::DarkBlue);
    break;
  case 14:
    chase(leds, numLeds, CRGB::Red);
    break;
  case 15:
    chase(leds, numLeds, CRGB::White);
    break;
  case 16:
    chase(leds, numLeds, CRGB::Cyan);
    break;
  case 17:
    chase(leds, numLeds, CRGB::Purple);
    break;

  case 50:
    fire(leds, numLeds, WoodFireColors_p);
    break;
  case 51:
    fire(leds, numLeds, CopperFireColors_p);
    break;
  case 52:
    fire(leds, numLeds, AlcoholFireColors_p);
    break;
  case 53:
    fire(leds, numLeds, SodiumFireColors_p);
    break;

  case 81:
    colorTwinkle(leds, numLeds, CRGB::Green);
    break;
  case 82:
    colorTwinkle(leds, numLeds, CRGB::Gold);
    break;
  case 83:
    colorTwinkle(leds, numLeds, CRGB::DarkBlue);
    break;
  case 84:
    colorTwinkle(leds, numLeds, CRGB::Red);
    break;
  case 85:
    colorTwinkle(leds, numLeds, CRGB::White);
    break;
  case 86:
    colorTwinkle(leds, numLeds, CRGB::Cyan);
    break;
  case 87:
    colorTwinkle(leds, numLeds, CRGB::Purple);
    break;

  case 91:
    rioSpin(leds, numLeds);
    break;
  case 92:
    rioDisco(leds, numLeds);
    break;
  case 93:
    rioFlag(leds, numLeds);
    break;

  case 97:
    hazards(leds, numLeds);
    break;

  case 98:
    // quick white strobe - flashes multiple times because TX resends mode 3 times :-|
    fill_solid(leds, numLeds, CRGB::White);
    FastLED.show();
    FastLED.delay(30);
    FastLED.clear();
    ledMode = currentMode; // reinstate the previous mode
    break;

  case 99:
    rainbow(leds, numLeds);
    break;

  case 199:
    nineninenine(leds, numLeds);
    break;


  // ERROR MODES
  case 253:
    // file failure
    showStatus(leds, CRGB::DarkMagenta);
    break;

  case 254:
    // radio failure
    showStatus(leds, CRGB::Red);
    break;

  default:
    // unknown mode
    showStatus(leds, CRGB::Grey);
    break;

  }

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}