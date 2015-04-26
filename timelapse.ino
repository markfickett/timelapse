/**
 * Run time lapse camera, monitoring environment and battery stats.
 */

#include "DHT22.h"

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
#define DHT22_PIN  2

struct SensorData {
  float humidityPercent;
  float temperatureCelsius;
};

struct SensorData sensorData;
DHT22 dht(DHT22_PIN);
DHT22_ERROR_t dhtError;

void setup() {
  Serial.begin(57600);
}

void loop() {
  delay(2000);
  readTemperatureAndHumidity();
  recordData();
}

void readTemperatureAndHumidity() {
  if ((dhtError = dht.readData()) == DHT_ERROR_NONE) {
    sensorData.humidityPercent = dht.getHumidity();
    sensorData.temperatureCelsius = dht.getTemperatureC();
  } else {
    Serial.print("DHT22 Error: ");
    Serial.println(dhtError);
    sensorData.humidityPercent = NAN;
    sensorData.temperatureCelsius = NAN;
  }
}

void recordData() {
  Serial.print("Temperature:\t");
  if (isnan(sensorData.temperatureCelsius)) {
    Serial.print("NaN");
  } else {
    Serial.print(sensorData.temperatureCelsius);
  }
  Serial.print("\tHumidity:\t");
  if (isnan(sensorData.humidityPercent)) {
    Serial.print("NaN");
  } else {
    Serial.print(sensorData.humidityPercent);
  }
  Serial.println();
}
