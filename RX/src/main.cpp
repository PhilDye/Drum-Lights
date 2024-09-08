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
uint8_t max_bright = 255;   // Overall brightness definition, could be changed on the fly
struct CRGB leds[MAX_LEDS]; // The array of leds, one for each led in the strip
uint8_t ledMode = 0;        // The currently active pattern
long NUM_LEDS = MAX_LEDS;  // To be read from config later

void (*resetFunc)(void) = 0; // declare reset function @ address 0

void showStatus(CRGB leds[], CRGB color)
{
  FastLED.clear();
  leds[1] = color;
}

#include "prototypes.h"

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
  const size_t bufferLen = 80;
  char buffer[bufferLen];

  const char *filename = "/config.ini";

  // Mount the SPIFFS
  if (!SPIFFS.begin())
    while (1)
      Serial.println("SPIFFS.begin() failed");
  
  SPIFFSIniFile ini(filename);
  if (!ini.open())
  {
    Serial.print("ini file ");
    Serial.print(filename);
    Serial.println(" does not exist");
    // Cannot do anything else
    while (1)
      ;
  }

  // Check the file is valid. This can be used to warn if any lines
  // are longer than the buffer.
  if (!ini.validate(buffer, bufferLen))
  {
    Serial.print("ini file ");
    Serial.print(ini.getFilename());
    Serial.print(" not valid: ");
    // Cannot do anything else
    while (1)
      ;
  }

  if (ini.getValue("leds", "count", buffer, bufferLen, NUM_LEDS)) {
    Serial.print("Got NUM_LEDS from config: ");
    Serial.println(NUM_LEDS);
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

  {

  }
  else

  Serial.print("Setting up radio...");
  // initialize the transceiver on the SPI bus
  if (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    for (size_t i = 0; i < 5; i++)
    {
      // flashing red
      showStatus(leds, CRGB::Red);
      delay(250);
      FastLED.clear(true);
      delay(250);
      showStatus(leds, CRGB::Red);
      delay(250);
      FastLED.clear(true);
      delay(250);
    }

    resetFunc();
  }

  radio.openReadingPipe(1, address);
  radio.setAutoAck(false);
  radio.startListening(); // put radio in TX mode
  radio.printPrettyDetails();  // (larger) function that prints human readable data
  Serial.println("Done");

  Serial.print("Ready...");

  showStatus(leds, CRGB::DarkGreen); // show we're ready
  Serial.println("Done");
}

void readRadio()
{
  uint8_t pipe;

  if (radio.available(&pipe))
  { // is there a payload?
    uint8_t bytes = radio.getPayloadSize();
    radio.read(&ledMode, bytes); // fetch payload from FIFO
    Serial.print("Radio RX data: ");
    Serial.println(ledMode);

    // if (ledMode > 47)
    // {
    //   ledMode = ledMode - 48; // seems to be an offset?!
    // }

    Serial.print("ledMode: ");
    Serial.println(ledMode);

    FastLED.clear();
  }
}

void loop()
{
  // Add entropy to random number generator; we use a lot of it
  random16_add_entropy(random());

  uint8_t currentMode = ledMode;

  readRadio();

  switch (ledMode)
  {
    case 11:
      chase(CRGB::Green, leds, NUM_LEDS);
      break;
    case 12:
      chase(CRGB::Gold, leds, NUM_LEDS);
      break;
    case 13:
      chase(CRGB::DarkBlue, leds, NUM_LEDS);
      break;
    case 14:
      chase(CRGB::Red, leds, NUM_LEDS);
      break;
    case 15:
      chase(CRGB::White, leds, NUM_LEDS);
      break;
    case 16:
      chase(CRGB::Cyan, leds, NUM_LEDS);
      break;
    case 17:
      chase(CRGB::Magenta, leds, NUM_LEDS);
      break;

    case 81:
      colorTwinkle(CRGB::Green, leds, NUM_LEDS);
      break;
    case 82:
      colorTwinkle(CRGB::Gold, leds, NUM_LEDS);
      break;
    case 83:
      colorTwinkle(CRGB::DarkBlue, leds, NUM_LEDS);
      break;
    case 84:
      colorTwinkle(CRGB::Red, leds, NUM_LEDS);
      break;
    case 85:
      colorTwinkle(CRGB::White, leds, NUM_LEDS);
      break;
    case 86:
      colorTwinkle(CRGB::Cyan, leds, NUM_LEDS);
      break;
    case 87:
      colorTwinkle(CRGB::Magenta, leds, NUM_LEDS);
      break;

    case 91:
      rioSpin(leds, NUM_LEDS);
      break;
    case 92:
      rioDisco(leds, NUM_LEDS);
      break;
    case 93:
      rioFlag(leds, NUM_LEDS);
      break;

    case 97:
      hazards(leds, NUM_LEDS);
      break;

    case 98:
      // quick white strobe - flashes multiple times because TX resends mode 3 times :-|
      fill_solid(leds, NUM_LEDS, CRGB::White);
      FastLED.show();
      FastLED.delay(30);
      FastLED.clear();
      ledMode = currentMode;    // reinstate the previous mode
      break;

    case 99:
      rainbow(leds, NUM_LEDS);
      break;

    case 199:
      nineninenine(leds, NUM_LEDS);
      break;

    default:
      showStatus(leds, CRGB::HotPink);
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
  }

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}