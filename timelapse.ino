/**
 * Run time lapse camera, monitoring environment and battery stats.
 *
 * Overall, the system draws (@12v) 0.7mA while idle, 14mA during data
 * sampling/writes, and ? when triggering the camera.
 */

#include "Wire.h"
#include "DHT22.h"
#include "RTClib.h"
#include "SdFat.h"
#include "Narcoleptic.h"

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
// Draws .05mA between reads, 1.66mA while reading.
#define DHT22_PIN  8

#define PHOTO_INTERVAL_SECONDS 60 * 5
// Increase this interval if DHT22 says DHT_ERROR_TOOQUICK (7).
#define SLEEP_INTERVAL_MILLIS 5 * 1000

// 12v (battery) power supply for the 9V regulator powering the camera.
#define PIN_CAMERA_POWER_SUPPLY 9
// Red wire to camera via opto-isolator.
#define PIN_CAMERA_POWER_ON 3
// Yellow wire to camera via opto-isolator.
#define PIN_CAMERA_SHUTTER 2
#define PIN_CAMERA_STAY_ON_SWITCH 4

#define PIN_DIVIDED_VCC A0
#define PIN_DIVIDED_PV  A1
// Resistor values for battery and photovoltaics. Each pair may be scaled
// together arbitrarily (ex: 4.7 and 1.2 or 47 and 12).
// The voltage dividers draw about 0.02mA each.
#define VCC_DIVIDER_SRC 1.000 + 0.992
#define VCC_DIVIDER_GND 0.272
#define PV_DIVIDER_SRC  0.997 + 0.996
#define PV_DIVIDER_GND  0.272
#define AREF 2.048

#define PIN_SPI_CHIP_SELECT_REQUIRED 10
// The SD library requires, for SPI communication:
// 11 MOSI (master/Arduino output, slave/SD input)
// 12 MISO (master input, slave output)
// 13 CLK (clock)
// Current draw is card dependant. forum.arduino.cc/index.php?topic=149504.15
// Crucial 4GB class 2: 0.06mA sleep between writes, 7.5mA writes.
// SanDisk 4GB class 4: 0.10mA sleep, 4.8mA writes.
// SanDisk ExtremePlus 32GB: 4.32mA sleep, 8mA writes.
// Kinsington 16GB: 16mA sleep, 22mA writes.

struct SensorData {
  DateTime now;
  float humidityPercent;
  float temperatureCelsius;
  int dividedVcc;
  int dividedPhotoVoltaic;
};

struct SensorData sensorData;
DHT22 dht(DHT22_PIN);
DHT22_ERROR_t dhtError;
uint32_t nextPhotoSeconds;

// Chronodot https://www.adafruit.com/products/255 and guide
// learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/wiring-it-up
// Using https://github.com/adafruit/RTClib
// It draws 0.1mA consistently while 3.3 vcc is supplied.
RTC_DS1307 rtc;

char buf[128]; // filenames and data log lines
SdFat sd;
SdFile logFile;

void setup() {
  Serial.begin(57600);
  // This must be called before analogRead if voltage is applied to the AREF
  // pin, or else the board may be damaged.
  analogReference(EXTERNAL);

  Wire.begin();
  rtc.begin();
  //setTime();
  setUpCameraPins();

  readTime();
  nextPhotoSeconds = sensorData.now.unixtime() + PHOTO_INTERVAL_SECONDS;
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
  lowPowerSleepMillis(SLEEP_INTERVAL_MILLIS);

  readTime();
  readTemperatureAndHumidity();
  readVoltages();
  printData();
  writeData();
  if (sensorData.now.unixtime() >= nextPhotoSeconds) {
    triggerCamera();
    while (sensorData.now.unixtime() >= nextPhotoSeconds) {
      nextPhotoSeconds += PHOTO_INTERVAL_SECONDS;
    }
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

void readVoltages() {
  sensorData.dividedVcc = analogRead(PIN_DIVIDED_VCC);
  sensorData.dividedPhotoVoltaic = analogRead(PIN_DIVIDED_PV);
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

  printVoltage(
      F("\tVcc "), sensorData.dividedVcc, VCC_DIVIDER_SRC, VCC_DIVIDER_GND);
  printVoltage(
      F("\tPV "), sensorData.dividedPhotoVoltaic, PV_DIVIDER_SRC,
      PV_DIVIDER_GND);

  Serial.println();
}

void printVoltage(
    const __FlashStringHelper* prefix,
    int analogReading,
    float rToSource,
    float rToGround) {
  Serial.print(prefix);
  Serial.print(analogReading);
  Serial.print(F("/1023 "));
  // 3.3 * A0 / 1023 = Vmeasured
  Serial.print(analogReading * AREF / 1024);
  Serial.print(F("v measured => "));
  // Vsrc / (R1 + R2) = Vmeasured / R2
  // Vsrc = (3.3 * A0 * (R1 + R2)) / (1023 * R2)
  Serial.print(
      (analogReading * AREF * (rToSource + rToGround))
      / (1023 * rToGround));
  Serial.print(F("v"));
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
      << ","
      << sensorData.dividedPhotoVoltaic
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

void lowPowerSleepMillis(int millis) {
  // This requires replacing millis() in DHT22.cpp with Narcoleptic.millis().
  Narcoleptic.disableTimer1();
  Narcoleptic.disableTimer2();
  Narcoleptic.disableSerial();
  Narcoleptic.disableADC();
  Narcoleptic.disableWire();
  Narcoleptic.disableSPI();

  Narcoleptic.delay(millis);

  Narcoleptic.enableTimer1();
  Narcoleptic.enableTimer2();
  Narcoleptic.enableSerial();
  Narcoleptic.enableADC();
  Narcoleptic.enableWire();
  Narcoleptic.enableSPI();
}
