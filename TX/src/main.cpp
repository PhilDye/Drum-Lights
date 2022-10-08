/* Drum Lights transmitter
 *
 * by Phil Dye <phil@phildye.org>
 * for Swan Samba Band <https://www.swansamba.co.uk>
 *
 * Works with the Drum Lights Receiver
 * to send desired colour/pattern commands from a web UI
 * by nRF24L01 radio module
 *
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <secrets.h>
#include <markup.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Setup the network
const char* ssid = WIFI_SSID;     // Set in build environment
const char* password = WIFI_PASS; // Set in build environment
const byte DNS_PORT = 53;
const byte HTTP_PORT = 80;

// Setup the servers
AsyncWebServer webServer(HTTP_PORT);
DNSServer dnsServer;

// Setup the nRF24L01 radio
#define NRF24L01_PIN_CE	17
#define NRF24L01_PIN_CS	5
RF24 radio(NRF24L01_PIN_CE, NRF24L01_PIN_CS);
const byte address[6] = "00001";


int8_t LEDMode = 0;

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");

    response->printf(html_head);
    response->print("<body>");
    response->print("<h1>Drum LED Control</h1>");
    response->printf("<p style='color:white'>Open <a href='http://%s' style='color:white'>LED Control</a></p>", WiFi.softAPIP().toString().c_str());
    response->print("</body></html>");
    request->send(response);
  }
};

String SendHTML(byte ledMode){

  String page = String(html_head);
  page +="<body>";
  // page +="<h1>Drum LED Control</h1>";

  switch (ledMode)
  {
  case 1:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 2:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 3:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=0\">Yellow</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 4:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 5:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 6:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 7:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 11:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 12:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 13:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 14:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 15:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 16:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 17:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 81:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 82:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 83:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 84:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 85:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 86:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 87:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 91:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 92:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 93:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">Off</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;

  case 99:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-on\" href=\"/mode?mode=0\">off</a>";
    page +="</div>";
    break;

  default:
    page +="<div class=\"col\">";
    page +="<h3>Solid</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=1\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=2\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=3\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=4\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=5\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=6\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=7\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Chase</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=11\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=12\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=13\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=14\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=15\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=16\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=17\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"col\">";
    page +="<h3>Twinkle</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=81\">Green</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=82\">Yellow</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=83\">Blue</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=84\">Red</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=85\">White</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=86\">Cyan</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=87\">Magenta</a>";
    page +="</div>";
    page +="<div class=\"row\">";
    page +="<h3>Effects</h3>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=91\">Spin</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=92\">Disco</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=93\">Flag</a>";
    page +="<a class=\"button button-off\" href=\"/mode?mode=99\">Rainbow</a>";
    page +="</div>";
    break;
  }
  
  page +="</body>";
  page +="</html>";
  return page;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


void setup() {
  Serial.begin(115200);
   while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {} // hold in infinite loop
  }

  radio.openWritingPipe(address);
  // Set the PA Level low to try preventing power supply related problems
  // because these examples are likely run with nodes in close proximity to
  // each other.
  radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default.
  radio.stopListening();          // put radio in TX mode

  radio.printPrettyDetails(); // (larger) function that prints human readable data


  WiFi.softAP(ssid, password);
  delay(100);

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", SendHTML(LEDMode)); 
  });

  webServer.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
    String mode;
    if (request->hasParam("mode")) {
        mode = request->getParam("mode")->value();
    } else {
        request->send(400, "text/plain", "No mode sent");
    }
    LEDMode = mode.toInt();
    Serial.println("Setting Mode " + mode);

    radio.write(&LEDMode, sizeof(mode));

    request->send(200, "text/html", SendHTML(LEDMode));
  });

  webServer.onNotFound(notFound);
    
  // Setup a captive portal, responding to all DNS requests with the ESP's IP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS server started");
 
  webServer.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  //only when requested from AP

  webServer.begin();
  Serial.println("HTTP server started on " + WiFi.softAPIP().toString());

}

void loop() {
  dnsServer.processNextRequest();
}



