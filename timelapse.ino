/**
 * Run time lapse camera, monitoring environment and battery stats.
 */

#include "Wire.h"
#include "DHT22.h"
#include "RTClib.h"

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
#define DHT22_PIN  2

#define PHOTO_INTERVAL_SECONDS 60 * 5

#define PIN_CAMERA_POWER_SUPPLY 5
#define PIN_CAMERA_POWER_ON 4
#define PIN_CAMERA_SHUTTER 3
#define PIN_CAMERA_STAY_ON_SWITCH 6

#define PIN_DIVIDED_VCC A0

struct SensorData {
  DateTime now;
  float humidityPercent;
  float temperatureCelsius;
  int dividedVcc;
};

struct SensorData sensorData;
DHT22 dht(DHT22_PIN);
DHT22_ERROR_t dhtError;
uint32_t lastPhotoSeconds;

// Chronodot https://www.adafruit.com/products/255 and guide
// learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/wiring-it-up
// Using https://github.com/adafruit/RTClib
RTC_DS1307 rtc;

void setup() {
  Serial.begin(57600);

  Wire.begin();
  rtc.begin();
  //setTime();
  setUpCameraPins();
  lastPhotoSeconds = 0;
}

void setTime() {
  DateTime now = DateTime(__DATE__, __TIME__); // time at compilation
  now = now + TimeSpan(0, 4, 0, 0); // EST => UTC
  rtc.adjust(now);
}

void setUpCameraPins() {
  pinMode(PIN_CAMERA_POWER_SUPPLY, OUTPUT);
  pinMode(PIN_CAMERA_POWER_ON, OUTPUT);
  pinMode(PIN_CAMERA_SHUTTER, OUTPUT);
  pinMode(PIN_CAMERA_STAY_ON_SWITCH, INPUT_PULLUP);
}

void loop() {
  delay(2000);
  readTime();
  readTemperatureAndHumidity();
  readVcc();
  recordData();
  if (sensorData.now.unixtime() - lastPhotoSeconds > PHOTO_INTERVAL_SECONDS) {
    triggerCamera();
    lastPhotoSeconds = sensorData.now.unixtime();
  }
  if (digitalRead(PIN_CAMERA_STAY_ON_SWITCH) == LOW) {
    digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
    delay(100);
    digitalWrite(PIN_CAMERA_POWER_ON, HIGH);
    delay(100);
    while (digitalRead(PIN_CAMERA_STAY_ON_SWITCH) == LOW) {
      Serial.println("Waiting with camera on.");
      delay(10000);
    }
    delay(100);
    digitalWrite(PIN_CAMERA_POWER_ON, LOW);
    delay(1000);
    digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
  }
}

void readTime() {
  sensorData.now = rtc.now();
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

void readVcc() {
  sensorData.dividedVcc = analogRead(PIN_DIVIDED_VCC);
}

void recordData() {
  Serial.print("t:\t");
  Serial.print(sensorData.now.unixtime());
  Serial.print("\t");
  Serial.print(sensorData.now.year(), DEC);
  Serial.print('/');
  Serial.print(sensorData.now.month(), DEC);
  Serial.print('/');
  Serial.print(sensorData.now.day(), DEC);
  Serial.print(' ');
  Serial.print(sensorData.now.hour(), DEC);
  Serial.print(':');
  Serial.print(sensorData.now.minute(), DEC);
  Serial.print(':');
  Serial.print(sensorData.now.second(), DEC);

  Serial.print("\tTemperature:\t");
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

  Serial.print("\tVcc ");
  Serial.print(sensorData.dividedVcc);
  Serial.print("/1023 ");
  // 3.3 * A0 / 1023 = Vmeasured
  Serial.print(sensorData.dividedVcc * 3.3 / 1024);
  Serial.print("v measured => ");
  // Vbat / (R1 + R2) = Vmeasured / R2
  // Vbat = (3.3 * Vmeasured * (R1 + R2)) / (1023 * R2)
  Serial.print(
      (sensorData.dividedVcc * 3.3 * (1.190 + 4.689))
      / (1023 * 1.190));
  Serial.print("v");

  Serial.println();
}

void triggerCamera() {
  Serial.println("Triggering camera.");
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
  delay(100);
  digitalWrite(PIN_CAMERA_POWER_ON, HIGH);
  delay(4000); // Camera initialization.

  digitalWrite(PIN_CAMERA_SHUTTER, HIGH);
  Serial.println("Triggering camera shutter.");
  delay(100);
  digitalWrite(PIN_CAMERA_SHUTTER, LOW);
  delay(10000); // Data write + long exposure.

  digitalWrite(PIN_CAMERA_POWER_ON, LOW);
  delay(100);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
}
