#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define RX_PIN 4
#define TX_PIN 5

BLEClient *pClient = nullptr;
bool connected = false;

void setup()
{
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    BLEDevice::init("S6APP1");
}

void loop()
{
    if (Serial2.available())
    {
        String line = Serial2.readStringUntil('\n');
        Serial.print("Received: ");
        Serial.println(line);
    }

    delay(5000);

    if (!connected)
    {
        BLEDevice::getScan()->start(0);
        pClient = BLEDevice::createClient();
        if (pClient->connect(BLEAddress("Station météo ESP32 - :)")))
        {
            Serial.println("Connected to server");
            connected = true;
        }
    }

    if (connected && pClient->isConnected())
    {
        BLERemoteService *pRemoteService = pClient->getService(BLEUUID((uint16_t)0x181A));
        Serial.println("Got service");
        if (pRemoteService)
        {
            BLERemoteCharacteristic *pRemoteChar = pRemoteService->getCharacteristic(BLEUUID((uint16_t)0x2b05));
            if (pRemoteChar)
            {
                Serial.println("Sending ping");
                uint8_t value = 0x01;
                pRemoteChar->writeValue(value);
            }
        }
    }
}