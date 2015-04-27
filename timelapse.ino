/**
 * Run time lapse camera, monitoring environment and battery stats.
 */

#include "Wire.h"
#include "DHT22.h"
#include "RTClib.h"
#include "SdFat.h"

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
#define DHT22_PIN  2

#define PHOTO_INTERVAL_SECONDS 60 * 5

#define PIN_CAMERA_POWER_SUPPLY 5
#define PIN_CAMERA_POWER_ON 4
#define PIN_CAMERA_SHUTTER 3
#define PIN_CAMERA_STAY_ON_SWITCH 6

#define PIN_DIVIDED_VCC A0

#define PIN_SPI_CHIP_SELECT_REQUIRED 10
// The SD library requires, for SPI communication:
// 11 MOSI (master/Arduino output, slave/SD input)
// 12 MISO (master input, slave output)
// 13 CLK (clock)

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

char buf[128]; // filenames and data log lines
SdFat sd;
SdFile logFile;

void setup() {
  Serial.begin(57600);

  Wire.begin();
  rtc.begin();
  //setTime();
  setUpCameraPins();

  readTime();
  lastPhotoSeconds = sensorData.now.unixtime();
  if (!sd.begin(PIN_SPI_CHIP_SELECT_REQUIRED, SPI_QUARTER_SPEED)) {
    sd.initErrorHalt();
  }
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
  printData();
  writeData();
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
      Serial.println(F("Waiting with camera on."));
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
    Serial.print(F("DHT22 Error: "));
    Serial.println(dhtError);
    sensorData.humidityPercent = NAN;
    sensorData.temperatureCelsius = NAN;
  }
}

void readVcc() {
  sensorData.dividedVcc = analogRead(PIN_DIVIDED_VCC);
}

void printData() {
  Serial.print(F("t: "));
  Serial.print(sensorData.now.unixtime());
  Serial.print(F(" "));
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

  Serial.print(F("\tTemperature: "));
  if (isnan(sensorData.temperatureCelsius)) {
    Serial.print(F("NaN"));
  } else {
    Serial.print(sensorData.temperatureCelsius);
    Serial.print(F(" C"));
  }
  Serial.print(F("\tHumidity: "));
  if (isnan(sensorData.humidityPercent)) {
    Serial.print(F("NaN"));
  } else {
    Serial.print(sensorData.humidityPercent);
    Serial.print(F(" %"));
  }

  Serial.print(F("\tVcc "));
  Serial.print(sensorData.dividedVcc);
  Serial.print(F("/1023 "));
  // 3.3 * A0 / 1023 = Vmeasured
  Serial.print(sensorData.dividedVcc * 3.3 / 1024);
  Serial.print(F("v measured => "));
  // Vbat / (R1 + R2) = Vmeasured / R2
  // Vbat = (3.3 * Vmeasured * (R1 + R2)) / (1023 * R2)
  Serial.print(
      (sensorData.dividedVcc * 3.3 * (1.190 + 4.689))
      / (1023 * 1.190));
  Serial.print(F("v"));

  Serial.println();
}

void writeData() {
  sprintf(
      buf,
      "%02d%02d%02d.csv",
      sensorData.now.year(),
      sensorData.now.month(),
      sensorData.now.day());
  if (!logFile.open(buf, O_CREAT | O_WRITE | O_AT_END)) {
    sd.errorHalt("Opening logFile failed. ");
  }

  obufstream ob(buf, sizeof(buf));
  ob << setprecision(2)
      << sensorData.now.unixtime()
      << ","
      << sensorData.temperatureCelsius
      << ","
      << sensorData.humidityPercent
      << ","
      << sensorData.dividedVcc
      << "\n";
  logFile.print(buf);

  if (!logFile.sync() || logFile.getWriteError()) {
    sd.errorHalt("Writing logFile failed. ");
  }
  logFile.close();
}

void triggerCamera() {
  Serial.println(F("Triggering camera."));
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
  delay(100);
  digitalWrite(PIN_CAMERA_POWER_ON, HIGH);
  delay(4000); // Camera initialization.

  digitalWrite(PIN_CAMERA_SHUTTER, HIGH);
  Serial.println(F("Triggering camera shutter."));
  delay(100);
  digitalWrite(PIN_CAMERA_SHUTTER, LOW);
  delay(10000); // Data write + long exposure.

  digitalWrite(PIN_CAMERA_POWER_ON, LOW);
  delay(100);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
}
