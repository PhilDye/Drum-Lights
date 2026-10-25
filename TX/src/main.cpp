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
#include <esp_wifi.h> //Used for mpdu_rx_disable android workaround
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <millisDelay.h>

#include <secrets.h>

// Setup the network
const byte DNS_PORT = 53;
const byte HTTP_PORT = 80;
const IPAddress localIP(192, 168, 4, 1);          // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(192, 168, 4, 1);        // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0); // no need to change: https://avinetworks.com/glossary/subnet-mask/
const String localIPURL = "http://192.168.4.1";   // a string version of the local IP with http, used for redirecting clients to your webpage
#define DNS_INTERVAL 30

// Setup the servers
AsyncWebServer webServer(HTTP_PORT);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

// Setup the nRF24L01 radio
#define NRF24L01_PIN_CE 17
#define NRF24L01_PIN_CS 5
RF24 radio(NRF24L01_PIN_CE, NRF24L01_PIN_CS);
const byte address[5] = {'R', 'x', 'A', 'A', '1'};
const byte RETRANSMITS = 5; // how many times we retransmit every message, for reliability in noisy RF environments

#define LED_BUILTIN 2

int CurrentMode = 0;

const unsigned long AUTO_TIME = 30000; // in mS
const int AUTO_MODE = -1;
const int STROBE_MODE = 98; // one-shot; the RX revert to their previous mode
millisDelay autoDelay; // the delay object

// Mode requests from the WebSocket (and later BLE) callbacks are queued here
// and applied on the loop task, so mode state and the radio have one owner
const UBaseType_t MODE_QUEUE_LENGTH = 8;
QueueHandle_t modeQueue;

const int autoModes[24] = {
    1, 2, 3, 4, 5, 6, 7, 8,
    11, 12, 13, 14, 15, 16, 17, 18,
    81, 82, 83, 84, 85, 86, 87, 88};

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
    explicit CaptiveRequestHandler(String redirectTargetURL) :
        targetURL("http://" + WiFi.softAPIP().toString() + redirectTargetURL)
    {
    }
    virtual ~CaptiveRequestHandler() {}

    const String targetURL;

    bool canHandle(AsyncWebServerRequest *request) override
    {
        // redirect if not in wifi client mode (through filter)
        // and request for different host (due to DNS * response)
        if (request->host() != WiFi.softAPIP().toString())
            return true;
        else
            return false;
    }

    void handleRequest(AsyncWebServerRequest *request) override
    {
        request->redirect(targetURL);
        log_d("Captive handler triggered. Requested %s%s -> redirecting to %s", request->host().c_str(), request->url().c_str(), targetURL.c_str());
    }
};

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
  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MAX is default
  radio.setAutoAck(false);
  radio.stopListening(); // put radio in TX mode

  radio.printPrettyDetails(); // (larger) function that prints human readable data
}

void setUpWebserver(AsyncWebServer &webServer, const IPAddress &localIP)
{
  	// Required
	webServer.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	webServer.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	webServer.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	webServer.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	webServer.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	webServer.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	webServer.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	webServer.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home
  
  webServer.addHandler(new CaptiveRequestHandler("/")).setFilter(ON_AP_FILTER); // only when requested from AP
  webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=600");;
  webServer.onNotFound([](AsyncWebServerRequest *request) { request->redirect(localIPURL); });

  webServer.begin();
}

size_t buildStateMessage(char *msg, size_t msgSize)
{
  const size_t size = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<size> json;
  json["mode"] = CurrentMode;
  json["auto"] = autoDelay.isRunning();

  return serializeJson(json, msg, msgSize);
}

// Tell every connected controller the current state
// (only WebSocket clients for now; BLE will hook in here too)
void notifyAll()
{
  char msg[64];
  size_t len = buildStateMessage(msg, sizeof(msg));
  ws.textAll(msg, len);

  Serial.printf("State broadcasted to WS: %s\n", msg);
}

void sendStateToClient(AsyncWebSocketClient *client)
{
  char msg[64];
  size_t len = buildStateMessage(msg, sizeof(msg));
  client->text(msg, len);

  Serial.printf("State sent to WS client #%u: %s\n", client->id(), msg);
}

void broadcastRF(int mode)
{
  for (size_t i = 0; i < RETRANSMITS; i++)
  {
    radio.write(&mode, sizeof(mode), true);
    delay(10);
  }
  Serial.printf("Mode #%d broadcasted to RF\n", mode);
}

// Set the mode on all receivers and tell all controllers; leaves auto alone
void sendMode(int mode)
{
  CurrentMode = mode;
  Serial.printf("CurrentMode set to #%d\n", CurrentMode);

  broadcastRF(CurrentMode);
  notifyAll();
}

// The single entry point for a mode requested by any controller
// (only call from the loop task; other tasks use requestMode())
void applyMode(int newMode)
{
  Serial.printf("Mode #%d requested\n", newMode);

  if (newMode == AUTO_MODE)
  {
    autoDelay.start(AUTO_TIME);
    Serial.printf("AUTO mode set ON\n");
    sendMode(autoModes[random(24)]);
    return;
  }

  if (newMode == STROBE_MODE)
  { // one-shot: RX revert by themselves, so CurrentMode and auto are unchanged
    broadcastRF(STROBE_MODE);
    notifyAll();
    return;
  }

  if (autoDelay.isRunning())
  {
    autoDelay.stop();
    Serial.printf("AUTO mode set OFF\n");
  }

  sendMode(newMode);
}

// Called from loop() when the auto delay expires
void nextAutoMode()
{
  // justFinished() stops the delay, so restart it before notifying,
  // otherwise clients are told auto is off
  autoDelay.repeat();
  Serial.println("autoDelay restarted");

  sendMode(autoModes[random(24)]);
}

// Queue a mode for applyMode() on the loop task; safe to call from any task, never blocks
void requestMode(int mode)
{
  // a strobe that would wait behind other requests is stale by the time it runs,
  // so drop it and keep the queue free for real mode changes
  if (mode == STROBE_MODE && uxQueueMessagesWaiting(modeQueue) > 0)
  {
    Serial.printf("Strobe dropped, requests already pending\n");
    return;
  }

  if (xQueueSend(modeQueue, &mode, 0) != pdTRUE)
    Serial.printf("Mode queue full, mode #%d dropped\n", mode);
}

void handleWSMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {

    const size_t size = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<size> json;
    DeserializationError err = deserializeJson(json, data);
    if (err)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.c_str());
      return;
    }

    requestMode(json["mode"].as<int>());
  }
}

void onWSEvent(AsyncWebSocket *server,
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
    sendStateToClient(client);
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    Serial.printf("WebSocket data received\n");
    handleWSMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onWSEvent);
  webServer.addHandler(&ws);
}

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP)
{
  // Respond to all DNS requests with the ESP's IP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.setTTL(3600);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS server started");
}

void startAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP, const IPAddress &subnetMask)
{
#define WIFI_CHANNEL 6

  // Set the WiFi mode to access point and station
  WiFi.mode(WIFI_MODE_AP);

  // Configure the soft access point with a specific IP and subnet mask
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

  // Start the soft access point with the given ssid, password, channel, max number of clients
  WiFi.softAP(ssid, password, WIFI_CHANNEL);

  // Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
  my_config.ampdu_rx_enable = false;
  esp_wifi_init(&my_config);
  esp_wifi_start();
  vTaskDelay(100 / portTICK_PERIOD_MS); // Add a small delay
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial)
  {
    // some boards need to wait to ensure access to serial over USB
  }

  modeQueue = xQueueCreate(MODE_QUEUE_LENGTH, sizeof(int));
  if (modeQueue == NULL)
  {
    Serial.println(F("Mode queue could not be created!!"));
    // hold in infinite loop
    while (1)
      digitalWrite(LED_BUILTIN, millis() % 1000 < 500 ? HIGH : LOW);
  }

  initRadio();

  if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LITTLEFS");
    return;
  }

  startAccessPoint(WIFI_SSID, WIFI_PASS, localIP, gatewayIP, subnetMask);

  setUpDNSServer(dnsServer, localIP);

  setUpWebserver(webServer, localIP);
  webServer.begin();

  initWebSocket();

  Serial.println("Ready; HTTP server started on " + WiFi.softAPIP().toString());
}

void loop()
{
  // Wait for a mode request instead of a fixed delay, so requests are applied
  // straight away while the loop still paces itself when idle
  int requested;
  if (xQueueReceive(modeQueue, &requested, pdMS_TO_TICKS(DNS_INTERVAL)) == pdTRUE)
  {
    do
    {
      applyMode(requested);
    } while (xQueueReceive(modeQueue, &requested, 0) == pdTRUE);
  }

  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (autoDelay.justFinished())
  {
    nextAutoMode();
  }
}
