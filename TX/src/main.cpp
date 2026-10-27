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

#ifdef BLE_SPIKE
#include <NimBLEDevice.h>
#include <esp_coexist.h>
#endif

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

// Reported to controllers in every state message, so an app can tell what it is talking to
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown" // normally set from git by version.py
#endif
const int PROTOCOL_VERSION = 1;  // bump on incompatible changes to the controller messages
const int CATALOGUE_VERSION = 1; // bump whenever modes are added, removed or renumbered, with "catalogue" in TX/app/src/modes.json

const unsigned long AUTO_TIME = 30000; // in mS
const int AUTO_MODE = -1;
const int STROBE_MODE = 98; // one-shot; the RX revert to their previous mode
millisDelay autoDelay; // the delay object

// Mode requests from the WebSocket (and later BLE) callbacks are queued here
// and applied on the loop task, so mode state and the radio have one owner
const UBaseType_t MODE_QUEUE_LENGTH = 8;
QueueHandle_t modeQueue;

// keep in step with "auto" in TX/app/src/modes.json (the web build checks)
const int autoModes[24] = {
    1, 2, 3, 4, 5, 6, 7, 8,
    11, 12, 13, 14, 15, 16, 17, 18,
    81, 82, 83, 84, 85, 86, 87, 88};

#ifdef SPIKE
// Phase 1 BLE coexistence spike (#19): counters for the soak test telemetry.
// Each has a single writer task, so plain volatile is enough for reporting
volatile uint32_t spikeWsCommands = 0;
volatile uint32_t spikeWsDisconnects = 0;
volatile uint32_t spikeBleCommands = 0;
volatile uint32_t spikeBleConnects = 0;
uint32_t spikeModesApplied = 0;
#endif

#ifdef BLE_SPIKE
// Throwaway GATT service from iOS/DESIGN.md section 3.2 (no passkey/bonding yet)
#ifndef BLE_NAME
#define BLE_NAME "Drum Lights TX" // override in secrets.h so neighbouring bands don't collide
#endif
#ifndef ADV_MIN_INTERVAL
#define ADV_MIN_INTERVAL 32 // 20 ms, in 0.625 ms units
#endif
#ifndef ADV_MAX_INTERVAL
#define ADV_MAX_INTERVAL 48 // 30 ms
#endif
#define BLE_SERVICE_UUID "9A9476F2-AE0D-406A-847A-D0FECAF34CE5"
#define BLE_STATE_UUID "E1EBAF93-75C1-4681-8F5B-D07D93D86FDE"
#define BLE_COMMAND_UUID "53F118A7-863E-429A-BF92-AC19864C8219"
#define BLE_INFO_UUID "B1AB44F5-A9A1-4152-852B-50A7D7EC8427"

NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleState = nullptr;

void requestMode(int mode);
#endif

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

const size_t STATE_MSG_SIZE = 128; // room for the fields below plus a long git describe

size_t buildStateMessage(char *msg, size_t msgSize)
{
  const size_t size = JSON_OBJECT_SIZE(5);
  StaticJsonDocument<size> json;
  json["mode"] = CurrentMode;
  json["auto"] = autoDelay.isRunning();
  json["proto"] = PROTOCOL_VERSION;
  json["fw"] = FIRMWARE_VERSION;
  json["catalogue"] = CATALOGUE_VERSION;

  return serializeJson(json, msg, msgSize);
}

#ifdef BLE_SPIKE
// State characteristic: int32 mode (LE) + uint8 flags (bit 0 = auto running)
void setBLEState()
{
  uint8_t value[5];
  int32_t mode = CurrentMode;
  memcpy(value, &mode, sizeof(mode)); // the ESP32 is little-endian
  value[4] = autoDelay.isRunning() ? 0x01 : 0x00;
  bleState->setValue(value, sizeof(value));
}

class SpikeServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
  {
    spikeBleConnects++;
    Serial.printf("BLE client %s connected (%u total)\n", connInfo.getAddress().toString().c_str(), server->getConnectedCount());
    NimBLEDevice::startAdvertising(); // keep advertising so more controllers can connect
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
  {
    Serial.printf("BLE client %s disconnected, reason %d\n", connInfo.getAddress().toString().c_str(), reason);
  }
};

class SpikeCommandCallbacks : public NimBLECharacteristicCallbacks
{
  // runs on the NimBLE host task, so hand the mode to the loop task
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
  {
    NimBLEAttValue value = characteristic->getValue();
    if (value.size() != sizeof(int32_t))
    {
      Serial.printf("BLE command ignored, %u bytes\n", value.size());
      return;
    }

    int32_t mode;
    memcpy(&mode, value.data(), sizeof(mode));
    spikeBleCommands++;
    requestMode(mode);
  }
};

void initBLE()
{
  NimBLEDevice::init(BLE_NAME);

  // #19: BLE connection *establishment* was unreliable with the WiFi AP running
  // (connected first try with WiFi compiled out, mostly failed to reach the
  // host at all with it on) — try biasing the radio's shared airtime toward
  // BT to see if that's airtime starvation rather than something deeper
  esp_err_t coexRc = esp_coex_preference_set(ESP_COEX_PREFER_BT);
  Serial.printf("esp_coex_preference_set(PREFER_BT) rc=%d\n", coexRc);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new SpikeServerCallbacks());
  bleServer->advertiseOnDisconnect(true); // defaults to false in NimBLE-Arduino 2.x

  NimBLEService *service = bleServer->createService(BLE_SERVICE_UUID);

  bleState = service->createCharacteristic(BLE_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  setBLEState();

  NimBLECharacteristic *command = service->createCharacteristic(BLE_COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
  command->setCallbacks(new SpikeCommandCallbacks());

  NimBLECharacteristic *info = service->createCharacteristic(BLE_INFO_UUID, NIMBLE_PROPERTY::READ);
  char infoMsg[STATE_MSG_SIZE];
  int infoLen = snprintf(infoMsg, sizeof(infoMsg), "{\"proto\":%d,\"fw\":\"%s\",\"catalogue\":%d}", PROTOCOL_VERSION, FIRMWARE_VERSION, CATALOGUE_VERSION);
  info->setValue((const uint8_t *)infoMsg, min((size_t)infoLen, sizeof(infoMsg) - 1));

  // a 128-bit UUID plus the name won't fit in 31 bytes, so the name goes in the scan response
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(service->getUUID());
  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_NAME);

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanData);

  // #19: advertise as fast as the spec allows for a connectable peripheral.
  // A central can only send its connection request during an advertising
  // event, so more events per second means more chances for one to land —
  // aimed at both the ~20% of attempts that fail and the multi-second
  // latency. Units are 0.625 ms, and 20 ms is the floor for connectable
  // undirected advertising. Override to compare against NimBLE's default.
  advertising->setMinInterval(ADV_MIN_INTERVAL);
  advertising->setMaxInterval(ADV_MAX_INTERVAL);

  // the real GATT registration: without this the characteristics above are
  // never handed to the NimBLE host, though advertising still runs either way
  bool serverStarted = bleServer->start();
  bool advertisingStarted = advertising->start();

  Serial.printf("BLE server start=%d advertising start=%d, advertising as %s\n",
                serverStarted, advertisingStarted, BLE_NAME);
}
#endif

// Tell every connected controller the current state
void notifyAll()
{
  char msg[STATE_MSG_SIZE];
  size_t len = buildStateMessage(msg, sizeof(msg));
  ws.textAll(msg, len);

  Serial.printf("State broadcasted to WS: %s\n", msg);

#ifdef BLE_SPIKE
  setBLEState();
  bleState->notify();
#endif
}

void sendStateToClient(AsyncWebSocketClient *client)
{
  char msg[STATE_MSG_SIZE];
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
#ifdef SPIKE
  spikeModesApplied++;
#endif

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

#ifdef SPIKE
    spikeWsCommands++;
#endif
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
#ifdef SPIKE
    spikeWsDisconnects++;
#endif
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

#ifdef SPIKE
const unsigned long TELEMETRY_TIME = 10000; // in mS
millisDelay telemetryDelay;
#endif

#ifdef BLE_SPIKE
// #19 workaround. A connection attempt the central abandons mid-handshake leaves
// the controller not advertising while the host still thinks it is: no GAP event
// is raised, isAdvertising() keeps returning true, and start() therefore no-ops
// with "advertising already active". Nothing in the API reports the real state,
// so there is nothing to detect — instead re-arm blind on a timer whenever no
// client is connected. stop() clears the stale host flag so the following
// start() actually reaches the controller, which does recover it (verified on
// the bench). The cost is a sub-millisecond advertising gap every interval, and
// a small chance of clipping a connection attempt that is in flight at exactly
// that moment; the central retries, so that is the cheaper risk.
const unsigned long ADV_REARM_TIME = 10000; // in mS
millisDelay advRearmDelay;

void rearmAdvertising()
{
  if (bleServer->getConnectedCount() > 0)
    return; // a live connection means advertising state is moot

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->stop();
  if (!adv->start())
    Serial.println("BLE advertising re-arm failed");
}
#endif

#ifdef SPIKE

// RF test: alternate strobe with the solid colours 1-8, so every step differs from the
// last and a receiver's log shows each change it missed (score with tools/spike/rf_score.py)
const int RF_TEST_STEPS = 200;
const unsigned long RF_TEST_TIME = 400; // in mS, between steps
millisDelay rfTestDelay;
int rfTestStep = 0;

int rfTestMode(int step)
{
  return step % 2 ? STROBE_MODE : 1 + (step / 2) % 8;
}

void printTelemetry()
{
  Serial.printf("SPIKE t=%lus heap=%u minheap=%u maxblock=%u ws=%u wsCmd=%u wsDisc=%u",
                millis() / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                ws.count(), spikeWsCommands, spikeWsDisconnects);
#ifdef BLE_SPIKE
  Serial.printf(" ble=%u bleConn=%u bleCmd=%u", bleServer->getConnectedCount(), spikeBleConnects, spikeBleCommands);
#endif
  Serial.printf(" applied=%u\n", spikeModesApplied);
}

void printSpikeBanner()
{
  Serial.printf("SPIKE boot reset=%d sketch=%u freesketch=%u fw=%s ble=%s\n",
                esp_reset_reason(), ESP.getSketchSize(), ESP.getFreeSketchSpace(), FIRMWARE_VERSION,
#ifdef BLE_SPIKE
                "on"
#else
                "off"
#endif
  );
  Serial.println("SPIKE serial commands: r = start RF test, x = stop RF test, t = telemetry, a = adv state, b = stop+start adv");
}

// Serial commands and periodic telemetry; called from loop()
void spikeLoop()
{
  while (Serial.available())
  {
    switch (Serial.read())
    {
    case 'r':
      rfTestStep = 0;
      rfTestDelay.start(RF_TEST_TIME);
      Serial.printf("RFTEST start steps=%d interval=%lums\n", RF_TEST_STEPS, RF_TEST_TIME);
      break;
    case 'x':
      rfTestDelay.stop();
      Serial.printf("RFTEST stopped at step %d\n", rfTestStep);
      break;
    case 't':
      printTelemetry();
      break;
#ifdef BLE_SPIKE
    // #19: after an aborted connection attempt kills advertising, does the host
    // even know? And can the application get it back without a reboot?
    case 'a':
    {
      NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
      Serial.printf("BLE isAdvertising=%d connected=%u; calling start()...\n",
                    adv->isAdvertising(), bleServer->getConnectedCount());
      bool started = adv->start(); // sequenced: argument evaluation order is unspecified
      Serial.printf("BLE start() returned %d, isAdvertising now=%d\n", started, adv->isAdvertising());
      break;
    }
    // the host's isAdvertising() goes stale (stays true) when the controller
    // stops after an aborted connect, so start() no-ops with "already active";
    // stop() first to clear the flag, so start() actually re-issues to the controller
    case 'b':
    {
      NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
      bool stopped = adv->stop(); // sequenced: argument evaluation order is unspecified
      bool started = adv->start();
      Serial.printf("BLE stop() returned %d, then start() returned %d, isAdvertising=%d\n",
                    stopped, started, adv->isAdvertising());
      break;
    }
#endif
    }
  }

  if (rfTestDelay.justFinished())
  {
    requestMode(rfTestMode(rfTestStep++));
    if (rfTestStep < RF_TEST_STEPS)
      rfTestDelay.repeat();
    else
      Serial.printf("RFTEST done steps=%d\n", rfTestStep);
  }

  if (telemetryDelay.justFinished())
  {
    telemetryDelay.repeat();
    printTelemetry();
  }

#ifdef BLE_SPIKE
  if (advRearmDelay.justFinished())
  {
    advRearmDelay.repeat();
    rearmAdvertising();
  }
#endif
}
#endif

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

#ifndef SPIKE_NO_WIFI
  startAccessPoint(WIFI_SSID, WIFI_PASS, localIP, gatewayIP, subnetMask);

  setUpDNSServer(dnsServer, localIP);

  setUpWebserver(webServer, localIP);
  webServer.begin();

  initWebSocket();
#endif

#ifdef BLE_SPIKE
  initBLE();
#endif

#ifdef SPIKE
  printSpikeBanner();
  telemetryDelay.start(TELEMETRY_TIME);
#endif

#ifdef BLE_SPIKE
  advRearmDelay.start(ADV_REARM_TIME);
#endif

#ifdef SPIKE_NO_WIFI
  Serial.println("Ready; WiFi AP disabled (SPIKE_NO_WIFI), BLE only");
#else
  Serial.println("Ready; HTTP server started on " + WiFi.softAPIP().toString());
#endif
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

#ifndef SPIKE_NO_WIFI
  dnsServer.processNextRequest();
  ws.cleanupClients();
#endif

  if (autoDelay.justFinished())
  {
    nextAutoMode();
  }

#ifdef SPIKE
  spikeLoop();
#endif
}
