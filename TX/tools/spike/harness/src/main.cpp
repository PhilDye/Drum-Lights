/* Drum Lights BLE + WiFi coexistence test harness (#19)
 *
 * Runs the same load as the TX (WiFi AP, captive DNS, AsyncWebServer with a
 * WebSocket, LittleFS, NimBLE GATT service) on a bare ESP32 dev board. The
 * nRF24 send is simulated with the same 5 x 10 ms blocking loop and the board's
 * RGB LED shows the current mode. Protocol, AP address and GATT UUIDs match
 * the TX, so ../soak.py and nRF Connect work against it unchanged.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#ifndef NO_BLE
#include <NimBLEDevice.h>
#endif

#include "page.h"

#ifndef AP_SSID
#define AP_SSID "DrumLights-Harness"
#endif
#ifndef AP_PASS
#define AP_PASS "drumlights" // at least 8 characters, or the AP won't start
#endif
#define WIFI_CHANNEL 6 // same as the TX

#ifndef BLE_NAME
#define BLE_NAME "Drum Lights Harness"
#endif
#define BLE_SERVICE_UUID "9A9476F2-AE0D-406A-847A-D0FECAF34CE5"
#define BLE_STATE_UUID "E1EBAF93-75C1-4681-8F5B-D07D93D86FDE"
#define BLE_COMMAND_UUID "53F118A7-863E-429A-BF92-AC19864C8219"
#define BLE_INFO_UUID "B1AB44F5-A9A1-4152-852B-50A7D7EC8427"

// the S3 DevKitC-1 LED is on GPIO 48 (38 on v1.1 boards); override with -DLED_PIN=38
#if !defined(LED_PIN) && defined(RGB_BUILTIN)
#define LED_PIN RGB_BUILTIN
#endif

const IPAddress localIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

const int PROTOCOL_VERSION = 1;
const int CATALOGUE_VERSION = 1;
const char *FIRMWARE = "harness";

const int AUTO_MODE = -1;
const int STROBE_MODE = 98;
const unsigned long AUTO_TIME = 30000;    // in mS
const unsigned long TELEMETRY_TIME = 10000; // in mS
const size_t BLOB_SIZE = 64 * 1024;       // a file to download, standing in for the web UI assets
const int RETRANSMITS = 5;

AsyncWebServer webServer(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
QueueHandle_t modeQueue;

int currentMode = 0;
bool autoRunning = false;
unsigned long autoStarted = 0;
unsigned long lastTelemetry = 0;

// each counter has a single writer task, so plain volatile is enough for reporting
volatile uint32_t wsCommands = 0;
volatile uint32_t wsDisconnects = 0;
volatile uint32_t httpRequests = 0;
volatile uint32_t bleCommands = 0;
volatile uint32_t bleConnects = 0;
volatile uint32_t bleDisconnects = 0;
volatile uint32_t queueDrops = 0;
uint32_t modesApplied = 0;

#ifndef NO_BLE
NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleState = nullptr;
#endif

// ---- simulated lights -------------------------------------------------------

void showColour(uint8_t r, uint8_t g, uint8_t b)
{
#ifdef LED_PIN
  neopixelWrite(LED_PIN, r / 4, g / 4, b / 4); // a quarter brightness is plenty on the bench
#endif
}

// colours in the same order as the RX blocks: 1-8, 11-18, 81-88
void showMode(int mode)
{
  static const uint8_t colours[9][3] = {
      {20, 0, 20}, {0, 255, 0}, {255, 180, 0}, {0, 0, 255}, {255, 0, 0},
      {255, 255, 255}, {0, 255, 255}, {255, 0, 255}, {255, 80, 0}};
  int i = mode > 0 ? mode % 10 : 0;
  if (i > 8)
    i = 0;
  showColour(colours[i][0], colours[i][1], colours[i][2]);
}

// stands in for the TX's broadcastRF(): same blocking time on the loop task
void simulateRF(int mode)
{
  for (int i = 0; i < RETRANSMITS; i++)
    delay(10);
}

// ---- state to controllers -----------------------------------------------------

size_t buildStateMessage(char *msg, size_t size)
{
  StaticJsonDocument<JSON_OBJECT_SIZE(6)> json;
  json["mode"] = currentMode;
  json["auto"] = autoRunning;
  json["proto"] = PROTOCOL_VERSION;
  json["fw"] = FIRMWARE;
  json["catalogue"] = CATALOGUE_VERSION;
  json["n"] = modesApplied;
  return serializeJson(json, msg, size);
}

void notifyAll()
{
  char msg[128];
  size_t len = buildStateMessage(msg, sizeof(msg));
  ws.textAll(msg, len);

#ifndef NO_BLE
  uint8_t value[5];
  int32_t mode = currentMode;
  memcpy(value, &mode, sizeof(mode)); // the ESP32 is little-endian
  value[4] = autoRunning ? 0x01 : 0x00;
  bleState->setValue(value, sizeof(value));
  bleState->notify();
#endif
}

// ---- one command path, applied on the loop task ---------------------------------

int randomAutoMode()
{
  static const int modes[] = {1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 81, 82, 83, 84, 85, 86, 87, 88};
  return modes[random(sizeof(modes) / sizeof(modes[0]))];
}

void setMode(int mode)
{
  currentMode = mode;
  simulateRF(mode);
  showMode(mode);
  notifyAll();
}

void applyMode(int mode)
{
  modesApplied++;

  if (mode == AUTO_MODE)
  {
    autoRunning = true;
    autoStarted = millis();
    setMode(randomAutoMode());
    return;
  }

  if (mode == STROBE_MODE)
  { // one-shot, like the RX: flash and go back
    simulateRF(mode);
    showColour(255, 255, 255);
    delay(30);
    showMode(currentMode);
    notifyAll();
    return;
  }

  autoRunning = false;
  setMode(mode);
}

// safe from any task, never blocks
void requestMode(int mode)
{
  if (mode == STROBE_MODE && uxQueueMessagesWaiting(modeQueue) > 0)
    return; // a strobe that would wait is stale, as on the TX
  if (xQueueSend(modeQueue, &mode, 0) != pdTRUE)
    queueDrops++;
}

// ---- WebSocket and HTTP ------------------------------------------------------

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
  {
    char msg[128];
    size_t n = buildStateMessage(msg, sizeof(msg));
    client->text(msg, n);
    Serial.printf("WS client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  }
  case WS_EVT_DISCONNECT:
    wsDisconnects++;
    Serial.printf("WS client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT))
      return;
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> json;
    if (deserializeJson(json, data, len))
      return;
    wsCommands++;
    requestMode(json["mode"].as<int>());
    break;
  }
  default:
    break;
  }
}

size_t buildStats(char *msg, size_t size)
{
  StaticJsonDocument<JSON_OBJECT_SIZE(16)> json;
  json["uptime"] = millis() / 1000;
  json["heap"] = ESP.getFreeHeap();
  json["minheap"] = ESP.getMinFreeHeap();
  json["maxblock"] = ESP.getMaxAllocHeap();
  json["ws"] = ws.count();
  json["wsCmd"] = wsCommands;
  json["wsDisc"] = wsDisconnects;
  json["http"] = httpRequests;
#ifndef NO_BLE
  json["ble"] = bleServer->getConnectedCount();
  json["bleConn"] = bleConnects;
  json["bleDisc"] = bleDisconnects;
  json["bleCmd"] = bleCommands;
#else
  json["ble"] = nullptr; // not compiled in
#endif
  json["applied"] = modesApplied;
  json["queueDrops"] = queueDrops;
  json["reset"] = (int)esp_reset_reason();
  return serializeJson(json, msg, size);
}

void setUpWebServer()
{
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    httpRequests++;
    request->send_P(200, "text/html", PAGE_HTML);
  });
  webServer.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
    httpRequests++;
    char msg[384];
    buildStats(msg, sizeof(msg));
    request->send(200, "application/json", msg);
  });
  webServer.on("/blob", HTTP_GET, [](AsyncWebServerRequest *request) {
    httpRequests++;
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/blob.bin", "application/octet-stream");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  // captive portal: everything else goes to the page
  webServer.onNotFound([](AsyncWebServerRequest *request) {
    httpRequests++;
    request->redirect("http://192.168.4.1/");
  });

  ws.onEvent(onWSEvent);
  webServer.addHandler(&ws);
  webServer.begin();
}

// writes the download file once; LittleFS is part of the load being tested
void setUpFiles()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS mount failed; /blob will 404");
    return;
  }
  File f = LittleFS.open("/blob.bin", "r");
  if (f && f.size() == BLOB_SIZE)
    return;
  f.close();

  f = LittleFS.open("/blob.bin", "w");
  uint8_t chunk[256];
  for (size_t i = 0; i < sizeof(chunk); i++)
    chunk[i] = i;
  for (size_t written = 0; written < BLOB_SIZE; written += sizeof(chunk))
    f.write(chunk, sizeof(chunk));
  f.close();
  Serial.printf("Wrote /blob.bin, %u bytes\n", BLOB_SIZE);
}

void startAccessPoint()
{
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(localIP, localIP, subnetMask);
  WiFi.softAP(AP_SSID, AP_PASS, WIFI_CHANNEL);

  // the TX's Android AMPDU workaround, kept so the WiFi setup matches
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  config.ampdu_rx_enable = false;
  esp_wifi_init(&config);
  esp_wifi_start();
  delay(100);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("WiFi AP \"%s\" on channel %d, http://%s/\n", AP_SSID, WIFI_CHANNEL, WiFi.softAPIP().toString().c_str());
}

// ---- BLE ----------------------------------------------------------------------

#ifndef NO_BLE
class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
  {
    bleConnects++;
    Serial.printf("BLE %s connected (%u total)\n", connInfo.getAddress().toString().c_str(), server->getConnectedCount());
    NimBLEDevice::startAdvertising(); // keep advertising so more controllers can connect
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
  {
    bleDisconnects++;
    Serial.printf("BLE %s disconnected, reason %d\n", connInfo.getAddress().toString().c_str(), reason);
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
  {
    NimBLEAttValue value = characteristic->getValue();
    if (value.size() != sizeof(int32_t))
      return;
    int32_t mode;
    memcpy(&mode, value.data(), sizeof(mode));
    bleCommands++;
    requestMode(mode);
  }
};

void startBLE()
{
  NimBLEDevice::init(BLE_NAME);
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService *service = bleServer->createService(BLE_SERVICE_UUID);
  bleState = service->createCharacteristic(BLE_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *command = service->createCharacteristic(BLE_COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
  command->setCallbacks(new CommandCallbacks());
  NimBLECharacteristic *info = service->createCharacteristic(BLE_INFO_UUID, NIMBLE_PROPERTY::READ);
  char infoMsg[96];
  int len = snprintf(infoMsg, sizeof(infoMsg), "{\"proto\":%d,\"fw\":\"%s\",\"catalogue\":%d}", PROTOCOL_VERSION, FIRMWARE, CATALOGUE_VERSION);
  info->setValue((const uint8_t *)infoMsg, len);

  uint8_t initial[5] = {0, 0, 0, 0, 0};
  bleState->setValue(initial, sizeof(initial));

  // a 128-bit UUID plus the name won't fit in 31 bytes, so the name goes in the scan response
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(service->getUUID());
  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_NAME);
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanData);
  advertising->start();
  Serial.println("BLE advertising as \"" BLE_NAME "\"");
}
#endif

// ---- main -------------------------------------------------------------------

void printTelemetry()
{
  char msg[384];
  buildStats(msg, sizeof(msg));
  Serial.printf("STATS %s\n", msg);
}

void setup()
{
  Serial.begin(115200);
  delay(1500); // give a USB CDC monitor time to attach and catch the boot lines

  Serial.printf("\nHarness boot: reset=%d ble=%s sketch=%u freesketch=%u heap=%u\n",
                esp_reset_reason(),
#ifndef NO_BLE
                "on",
#else
                "off",
#endif
                ESP.getSketchSize(), ESP.getFreeSketchSpace(), ESP.getFreeHeap());

  modeQueue = xQueueCreate(8, sizeof(int));
  showMode(0);

  setUpFiles();
  startAccessPoint();
  setUpWebServer();
#ifndef NO_BLE
  startBLE();
#endif

  Serial.printf("Ready, heap=%u. Serial: t = stats now\n", ESP.getFreeHeap());
  printTelemetry();
}

void loop()
{
  int requested;
  if (xQueueReceive(modeQueue, &requested, pdMS_TO_TICKS(30)) == pdTRUE)
  {
    do
      applyMode(requested);
    while (xQueueReceive(modeQueue, &requested, 0) == pdTRUE);
  }

  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (autoRunning && millis() - autoStarted >= AUTO_TIME)
  {
    autoStarted = millis();
    setMode(randomAutoMode());
  }

  while (Serial.available())
    if (Serial.read() == 't')
      printTelemetry();

  if (millis() - lastTelemetry >= TELEMETRY_TIME)
  {
    lastTelemetry = millis();
    printTelemetry();
  }
}
