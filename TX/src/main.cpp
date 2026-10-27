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

#ifdef TX_BLE
#include <NimBLEDevice.h>
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

// Setup the nRF24L01 radio, on the board's default SPI pins
// (classic ESP32: SCK 18, MISO 19, MOSI 23; S3: SCK 12, MISO 13, MOSI 11)
#if CONFIG_IDF_TARGET_ESP32S3
#define NRF24L01_PIN_CE 9
#define NRF24L01_PIN_CS 10
#else
#define NRF24L01_PIN_CE 17
#define NRF24L01_PIN_CS 5
#endif
RF24 radio(NRF24L01_PIN_CE, NRF24L01_PIN_CS);
const byte address[5] = {'R', 'x', 'A', 'A', '1'};
const byte RETRANSMITS = 5; // how many times we retransmit every message, for reliability in noisy RF environments

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
#define BOOT_BUTTON_PIN 0

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

#ifdef TX_BLE
// GATT service from iOS/DESIGN.md section 3.2
#ifndef BLE_NAME
#define BLE_NAME "Drum Lights TX" // override in secrets.h so neighbouring bands don't collide
#endif
#ifndef BLE_PASSKEY
#error "Define BLE_PASSKEY in secrets.h: the 6-digit PIN a phone enters to pair, e.g. #define BLE_PASSKEY 482916 (no leading zero, C reads that as octal)"
#endif
static_assert(BLE_PASSKEY >= 0 && BLE_PASSKEY <= 999999, "BLE_PASSKEY must be at most 6 digits");

#define BLE_SERVICE_UUID "9A9476F2-AE0D-406A-847A-D0FECAF34CE5"
#define BLE_STATE_UUID "E1EBAF93-75C1-4681-8F5B-D07D93D86FDE"
#define BLE_COMMAND_UUID "53F118A7-863E-429A-BF92-AC19864C8219"
#define BLE_INFO_UUID "B1AB44F5-A9A1-4152-852B-50A7D7EC8427"

const uint16_t ADV_MIN_INTERVAL = 32; // 20 ms in 0.625 ms units, the floor for connectable advertising
const uint16_t ADV_MAX_INTERVAL = 48; // 30 ms
const unsigned long CLEAR_BONDS_HOLD = 5000; // in mS, BOOT button

NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleState = nullptr;
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

#ifdef TX_BLE
// State characteristic: int32 mode (LE) + uint8 flags (bit 0 = auto running)
void setBLEState()
{
  uint8_t value[5];
  int32_t mode = CurrentMode;
  memcpy(value, &mode, sizeof(mode)); // the ESP32 is little-endian
  value[4] = autoDelay.isRunning() ? 0x01 : 0x00;
  bleState->setValue(value, sizeof(value));
}
#endif

// State broadcasts are coalesced: a change only marks the state dirty, and
// flushState() on the loop task sends the latest at most every STATE_INTERVAL.
// Sending one WebSocket message per change let a burst (strobe, a controller
// hammering commands) overflow AsyncWebSocket's per-client queue, which ended
// in an AsyncTCP panic in the #19 soak
const unsigned long STATE_INTERVAL = 100; // in mS
bool stateChanged = false; // not yet sent to anyone
bool wsBehind = false;     // a WebSocket client was skipped and still needs the latest
unsigned long lastStateSent = 0;

// Tell every connected controller the current state (sent from flushState())
void notifyAll()
{
  stateChanged = true;
}

// Called from loop()
void flushState()
{
  if (!(stateChanged || wsBehind) || millis() - lastStateSent < STATE_INTERVAL)
    return;
  lastStateSent = millis();

  char msg[STATE_MSG_SIZE];
  size_t len = buildStateMessage(msg, sizeof(msg));

  // skip a client whose queue is full instead of overflowing it; it gets the
  // latest state on a later pass once it has drained (the others get a harmless
  // repeat on those passes, still no more than one per STATE_INTERVAL)
  wsBehind = false;
  for (AsyncWebSocketClient *client : ws.getClients())
  {
    if (client->status() != WS_CONNECTED)
      continue;
    if (client->canSend())
      client->text(msg, len);
    else
      wsBehind = true;
  }

  if (!stateChanged)
    return; // a catch-up pass for a slow client; the rest already have this state
  stateChanged = false;

  Serial.printf("State broadcasted to WS: %s\n", msg);

#ifdef TX_BLE
  if (bleState) // null if setup() bailed out early
  {
    setBLEState();
    bleState->notify();
  }
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

#ifdef TX_BLE
class BLEServerEvents : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
  {
    Serial.printf("BLE client %s connected (%u total)\n", connInfo.getAddress().toString().c_str(), server->getConnectedCount());

    // ask for pairing now, so a new phone gets the PIN prompt on connect rather
    // than on its first command; a bonded phone just re-encrypts silently
    NimBLEDevice::startSecurity(connInfo.getConnHandle());

    // advertising stops on a connection; keep it going so more controllers can join
    if (server->getConnectedCount() < CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
      NimBLEDevice::startAdvertising();
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
  {
    Serial.printf("BLE client %s disconnected, reason %d\n", connInfo.getAddress().toString().c_str(), reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo &connInfo) override
  {
    if (connInfo.isEncrypted() && connInfo.isAuthenticated())
    {
      Serial.printf("BLE client %s paired%s\n", connInfo.getAddress().toString().c_str(), connInfo.isBonded() ? " and bonded" : "");
      return;
    }

    // wrong PIN or pairing cancelled: free the connection slot
    Serial.printf("BLE client %s failed to pair, disconnecting\n", connInfo.getAddress().toString().c_str());
    bleServer->disconnect(connInfo.getConnHandle());
  }
};

class BLECommandEvents : public NimBLECharacteristicCallbacks
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
    Serial.printf("BLE command from %s\n", connInfo.getAddress().toString().c_str());
    requestMode(mode);
  }
};

void initBLE()
{
  NimBLEDevice::init(BLE_NAME);

  // bonded, MITM-protected LE Secure Connections with a static passkey: the TX
  // "displays" BLE_PASSKEY (the operator knows it) and the phone enters it once
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(BLE_PASSKEY);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new BLEServerEvents());
  bleServer->advertiseOnDisconnect(true); // defaults to false in NimBLE-Arduino 2.x

  NimBLEService *service = bleServer->createService(BLE_SERVICE_UUID);

  bleState = service->createCharacteristic(BLE_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  setBLEState();

  // write with response, and only over an authenticated (passkey) link
  NimBLECharacteristic *command = service->createCharacteristic(BLE_COMMAND_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  command->setCallbacks(new BLECommandEvents());

  NimBLECharacteristic *info = service->createCharacteristic(BLE_INFO_UUID, NIMBLE_PROPERTY::READ);
  char infoMsg[STATE_MSG_SIZE];
  int infoLen = snprintf(infoMsg, sizeof(infoMsg), "{\"proto\":%d,\"fw\":\"%s\",\"catalogue\":%d}", PROTOCOL_VERSION, FIRMWARE_VERSION, CATALOGUE_VERSION);
  info->setValue((const uint8_t *)infoMsg, min((size_t)infoLen, sizeof(infoMsg) - 1));

  // registers the characteristics with the host; advertising runs without it,
  // but nothing could discover the service (#19)
  bleServer->start();

  // a 128-bit UUID plus the name won't fit in 31 bytes, so the name goes in the scan response
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(service->getUUID());
  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_NAME);

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanData);
  // a central can only connect during an advertising event, so advertise often
  advertising->setMinInterval(ADV_MIN_INTERVAL);
  advertising->setMaxInterval(ADV_MAX_INTERVAL);

  if (!advertising->start())
    Serial.println(F("BLE advertising failed to start"));

  Serial.printf("BLE advertising as %s, %d bonds stored\n", BLE_NAME, NimBLEDevice::getNumBonds());
}

// Safety net from #19. On the classic ESP32 an aborted connection attempt left
// the controller not advertising while the host still reported it was, with no
// event raised, so only a blind stop() then start() recovered it. The S3 did not
// show the bug in testing, but the re-arm is harmless, so it stays
const unsigned long ADV_REARM_TIME = 10000; // in mS
millisDelay advRearmDelay;

void rearmAdvertising()
{
  if (bleServer->getConnectedCount() >= CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
    return; // no room for another controller, so nothing to advertise for

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->stop();
  if (!adv->start())
    Serial.println(F("BLE advertising re-arm failed"));
}

// Forget every paired phone, e.g. after one is lost. Connected phones are
// dropped too, so none stays connected on a bond that no longer exists
void clearBonds()
{
  for (uint16_t handle : bleServer->getPeerDevices())
    bleServer->disconnect(handle);

  bool cleared = NimBLEDevice::deleteAllBonds();
  Serial.printf("BLE pairings cleared: %s\n", cleared ? "ok" : "FAILED");

  // acknowledge on the LED: three quick flashes
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
  }
}

// Called from loop(): clear the pairings once BOOT has been held for CLEAR_BONDS_HOLD
void checkBootButton()
{
  static bool released = false; // seen up since boot, so a pin held low at boot can't wipe the bonds
  static bool timing = false;
  static unsigned long pressedAt = 0;

  if (digitalRead(BOOT_BUTTON_PIN) == HIGH)
  {
    released = true;
    timing = false;
    return;
  }

  if (!released)
    return;

  if (!timing)
  {
    timing = true;
    pressedAt = millis();
    return;
  }

  if (millis() - pressedAt >= CLEAR_BONDS_HOLD)
  {
    clearBonds();
    released = false; // once per press
  }
}
#endif

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  // on USB CDC, Serial only becomes true once a computer opens the port, so an
  // unbounded wait would hang a TX powered from a battery pack; and don't let
  // writes block for want of a reader
  Serial.setTxTimeoutMs(0);
  for (unsigned long start = millis(); !Serial && millis() - start < 1000;)
    delay(10);
#endif

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

#ifdef TX_BLE
  initBLE();
  advRearmDelay.start(ADV_REARM_TIME);
#endif

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

  flushState();

#ifdef TX_BLE
  if (bleServer) // null if setup() bailed out early
  {
    if (advRearmDelay.justFinished())
    {
      advRearmDelay.repeat();
      rearmAdvertising();
    }

    checkBootButton();
  }
#endif
}
