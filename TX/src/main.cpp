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
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Setup the network
const char *ssid = WIFI_SSID;     // Set in build environment
const char *password = WIFI_PASS; // Set in build environment
const byte DNS_PORT = 53;
const byte HTTP_PORT = 80;

// Setup the servers
AsyncWebServer webServer(HTTP_PORT);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

// Setup the nRF24L01 radio
#define NRF24L01_PIN_CE 17
#define NRF24L01_PIN_CS 5
RF24 radio(NRF24L01_PIN_CE, NRF24L01_PIN_CS);
const byte address[5] = {'R','x','A','A','1'};

uint8_t LEDMode = 0;

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/html");

    response->print("<!DOCTYPE html <html>");
    response->print("<head>");
    response->print("<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'>");
    response->print("<title>Drum LED Control</title>");
    response->print("<link rel='stylesheet' href='/index.css' />");
    response->print("</head>");
    response->print("<body>");
    response->print("<h1>Drum LED Control</h1>");
    response->printf("<p style='color:white'><a href='http://%s' style='color:white'>Open LED Control</a></p>", WiFi.softAPIP().toString().c_str());
    response->print("</body></html>");
    request->send(response);  
  }
};

String templateProcessor(const String &var)
{
  return String(var == "STATE" && false ? "on" : "off");
}

void onRootRequest(AsyncWebServerRequest *request)
{
  request->send(SPIFFS, "/index.html", "text/html", false, templateProcessor);
}

void onNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


void initRadio()
{
  // initialize the transceiver on the SPI bus
  if (!radio.begin())
  {
    Serial.println(F("Radio hardware is not responding!!"));
    // hold in infinite loop
    while (1)
      digitalWrite(LED_BUILTIN, millis() % 1000 < 500 ? HIGH : LOW);
  }

  radio.openWritingPipe(address);
  // Set the PA Level to try preventing power supply related problems
  radio.setPALevel(RF24_PA_HIGH); // RF24_PA_MAX is default
  radio.setAutoAck(false);
  radio.stopListening(); // put radio in TX mode

  radio.printPrettyDetails(); // (larger) function that prints human readable data
}

void initSPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("Cannot mount SPIFFS volume...");
    while (1)
      digitalWrite(LED_BUILTIN, millis() % 500 < 250 ? HIGH : LOW);
  }
}

void initWebServer()
{
  webServer.on("/", onRootRequest);
  webServer.on("/generate_204", onRootRequest);   //Android captive portal check
  webServer.onNotFound(onNotFound);
  webServer.serveStatic("/", SPIFFS, "/");
  webServer.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP

  webServer.begin();
}

void notifyClients() {
    const size_t size = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<size> json;
    json["mode"] = LEDMode;

    char msg[32];
    size_t len = serializeJson(json, msg);
    ws.textAll(msg, len);
}

void handleMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        const size_t size = JSON_OBJECT_SIZE(1);
        StaticJsonDocument<size> json;
        DeserializationError err = deserializeJson(json, data);
        if (err) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
            return;
        }

        const long mode = json["mode"];

        Serial.printf("Received mode #%4ld\n", mode);

        LEDMode = (int)mode;

        Serial.printf("LEDMode set to #%d\n", LEDMode);

        radio.write(&LEDMode, sizeof(mode));
        notifyClients();

    }
}


void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len)
{

  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    Serial.printf("WebSocket data received\n");
    handleMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  webServer.addHandler(&ws);
}

void initCaptivePortal()
{
  // Setup a captive portal, responding to all DNS requests with the ESP's IP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS server started");
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial)
  {
    // some boards need to wait to ensure access to serial over USB
  }

  initRadio();
  initSPIFFS();

  WiFi.softAP(ssid, password);
  delay(100);

  initWebSocket();
  initWebServer();
  initCaptivePortal();

  Serial.println("Ready; HTTP server started on " + WiFi.softAPIP().toString());
}

void loop()
{
  dnsServer.processNextRequest();
  ws.cleanupClients();
}
