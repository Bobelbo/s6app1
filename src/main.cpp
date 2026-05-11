#include <DHT.h>
#include <Arduino.h>
#include <Adafruit_DPS310.h>
#include "SparkFun_Weather_Meter_Kit_Arduino_Library.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID (BLEUUID((uint16_t)0x181A))
BLEDescriptor envDectriptor(BLEUUID((uint16_t)0x2902));

BLECharacteristic windDirectionCharacteristic(BLEUUID((uint16_t)0x2A70), BLECharacteristic::PROPERTY_READ);
BLECharacteristic windSpeedCharacteristic(BLEUUID((uint16_t)0x2A71), BLECharacteristic::PROPERTY_READ);
BLECharacteristic rainfallCharacteristic(BLEUUID((uint16_t)0x2A78), BLECharacteristic::PROPERTY_READ);
BLECharacteristic temperatureCharacteristic(BLEUUID((uint16_t)0x2A6E), BLECharacteristic::PROPERTY_READ);
BLECharacteristic humidityCharacteristic(BLEUUID((uint16_t)0x2A6F), BLECharacteristic::PROPERTY_READ);
BLECharacteristic pressureCharacteristic(BLEUUID((uint16_t)0x2A6D), BLECharacteristic::PROPERTY_READ);
BLECharacteristic lightCharacteristic(BLEUUID((uint16_t)0x2B03), BLECharacteristic::PROPERTY_READ);

BLECharacteristic dirtyCharacteristic(BLEUUID((uint16_t)0x2B04), BLECharacteristic::PROPERTY_READ);
BLECharacteristic sendDataCharacteristic(BLEUUID((uint16_t)0x2B05), BLECharacteristic::PROPERTY_WRITE_NR);

#define NB_SENSORS 7

#define DHTTYPE DHT11
#define DHT_PIN 16

#define windDirectionPin 35
#define windSpeedPin 27
#define rainfallPin 23

#define LIGHT_PIN 34
float calculatedLux()
{
  int sensorValue = analogRead(LIGHT_PIN);
  if (sensorValue == 0)
    return 0;
  return 0.005 * pow(sensorValue, 1.32);
}

Adafruit_DPS310 dps;
Adafruit_Sensor *dps_temp = dps.getTemperatureSensor();
Adafruit_Sensor *dps_pressure = dps.getPressureSensor();

DHT dht(DHT_PIN, DHTTYPE);
SFEWeatherMeterKit weatherMeterKit(windDirectionPin, windSpeedPin, rainfallPin);

typedef struct Sensor
{
  const char *name;
  BLECharacteristic *characteristic;
  float (*getValue)();
} Sensor;


Sensor sensors[NB_SENSORS];
float currentData[NB_SENSORS] = {0, 0, 0, 0, 0, 0, 0};
bool sendData = false;
bool dirty = false;

void setup()
{
  Serial.begin(9600);

  BLEDevice::init("Station météo ESP32 - :)");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->removeService(pServer->getServiceByUUID(SERVICE_UUID));

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pService->addCharacteristic(&windDirectionCharacteristic);
  windDirectionCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&rainfallCharacteristic);
  rainfallCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&windSpeedCharacteristic);
  windSpeedCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&temperatureCharacteristic);
  temperatureCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&humidityCharacteristic);
  humidityCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&pressureCharacteristic);
  pressureCharacteristic.addDescriptor(&envDectriptor);
  pService->addCharacteristic(&lightCharacteristic);
  lightCharacteristic.addDescriptor(&envDectriptor);

  pService->addCharacteristic(&dirtyCharacteristic);
  pService->addCharacteristic(&sendDataCharacteristic);

  weatherMeterKit.begin();
  if (!dps.begin_I2C())
  {
    Serial.println("Failed to find DPS");
    while (1)
      yield();
  }

  dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
  dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);

  dps_temp->printSensorDetails();
  dps_pressure->printSensorDetails();

  sensors[0] = {"Wind direction (degrees)", &windDirectionCharacteristic, weatherMeterKit.getWindDirection};
  sensors[1] = {"Wind speed (kph)", &windSpeedCharacteristic, weatherMeterKit.getWindSpeed};
  sensors[2] = {"Total rainfall (mm)", &rainfallCharacteristic, weatherMeterKit.getTotalRainfall};
  sensors[3] = {"Temperature (C)", &temperatureCharacteristic, []()
                {
                  sensors_event_t temp_event;
                  dps_temp->getEvent(&temp_event);
                  return (temp_event.temperature + dht.readTemperature()) / 2;
                }};
  sensors[4] = {"Humidity (%)", &humidityCharacteristic, []()
                { return dht.readHumidity(); }};
  sensors[5] = {"Light (lux)", &lightCharacteristic, calculatedLux};
  sensors[6] = {"Pressure (Pa)", &pressureCharacteristic, []()
                {
                  sensors_event_t pressure_event;
                  dps_pressure->getEvent(&pressure_event);
                  return pressure_event.pressure * 100;
                }};

  pService->start();
  pServer->getAdvertising()->start();
}

void loop()
{
  Serial.println();
  for (int i = 0; i < NB_SENSORS; i++)
  {
    float value = sensors[i].getValue();
    Serial.print(sensors[i].name);
    Serial.print(": ");
    Serial.print(value, 1);
    char bleValue[8] = {0};
    sensors[i].characteristic->setValue(value);
    Serial.print("BLE value: ");
    Serial.println(bleValue);

    currentData[i] = value;
    dirty = true;
    dirtyCharacteristic.setValue((uint8_t *)&dirty, sizeof(bool));
  }

  sendData = sendDataCharacteristic.getValue()[0] == 1;

  if (sendData)
  {
    // Send data via UART

    dirty = false;
    sendData = false;
    dirtyCharacteristic.setValue((uint8_t *)&dirty, sizeof(bool));
    sendDataCharacteristic.setValue((uint8_t *)&sendData, sizeof(bool));
  }

  delay(1000);
}