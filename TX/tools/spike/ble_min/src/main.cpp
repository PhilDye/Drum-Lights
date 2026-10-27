/* Minimal NimBLE peripheral: does an aborted connection attempt stop the TX
 * advertising, and does the host still claim it is advertising afterwards?
 * See ../README.md (#19). Same service/characteristic UUIDs as the TX so the
 * existing ble_reliability.py works against this unchanged.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

#define BLE_NAME "Drum Lights TX"
#define BLE_SERVICE_UUID "9A9476F2-AE0D-406A-847A-D0FECAF34CE5"
#define BLE_STATE_UUID "E1EBAF93-75C1-4681-8F5B-D07D93D86FDE"
#define BLE_COMMAND_UUID "53F118A7-863E-429A-BF92-AC19864C8219"

NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleState = nullptr;
unsigned long lastReport = 0;

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
  {
    Serial.printf("connected (%u total)\n", server->getConnectedCount());
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
  {
    Serial.printf("disconnected, reason %d\n", reason);
  }
};

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
  }

  NimBLEDevice::init(BLE_NAME);
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  bleServer->advertiseOnDisconnect(true);

  NimBLEService *service = bleServer->createService(BLE_SERVICE_UUID);
  bleState = service->createCharacteristic(BLE_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  uint8_t initial[5] = {0, 0, 0, 0, 0};
  bleState->setValue(initial, sizeof(initial));
  service->createCharacteristic(BLE_COMMAND_UUID, NIMBLE_PROPERTY::WRITE);

  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(service->getUUID());
  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_NAME);

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);

  Serial.printf("server start=%d adv start=%d\n", bleServer->start(), adv->start());
  Serial.printf("MIN boot ble=on; commands: a = adv state + start(), b = stop()+start()\n");
}

void loop()
{
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();

  while (Serial.available())
  {
    switch (Serial.read())
    {
    case 'a':
      Serial.printf("isAdvertising=%d connected=%u\n", adv->isAdvertising(), bleServer->getConnectedCount());
      Serial.printf("start() returned %d, isAdvertising now=%d\n", adv->start(), adv->isAdvertising());
      break;
    case 'b':
      Serial.printf("stop() returned %d, start() returned %d, isAdvertising=%d\n",
                    adv->stop(), adv->start(), adv->isAdvertising());
      break;
    }
  }

  if (millis() - lastReport > 10000)
  {
    lastReport = millis();
    Serial.printf("MIN t=%lus isAdvertising=%d connected=%u heap=%u\n",
                  millis() / 1000, adv->isAdvertising(), bleServer->getConnectedCount(), ESP.getFreeHeap());
  }
}
