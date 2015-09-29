/**
 * Run time lapse camera, monitoring environment and battery stats.
 *
 * Overall, the system draws (@12v) 0.7mA while idle, 14mA during data
 * sampling/writes, and 200mA-2A when triggering the camera.
 */

#include "Wire.h"
#include "DHT22.h"
#include "RTClib.h"
#include "SdFat.h"
#include "Narcoleptic.h"

// Not the onboard status LED, which is used by the micro-SD card and obscured
// in this application.
#define PIN_STATUS_LED 6

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
// Draws .05mA between reads, 1.66mA while reading.
#define PIN_DHT22  8

// Increase this interval if DHT22 says DHT_ERROR_TOOQUICK (7). 5s is OK.
#define SLEEP_INTERVAL_MILLIS (10 * 1000)

// 12v (battery) power supply for the 9V regulator powering the camera.
#define PIN_CAMERA_POWER_SUPPLY 9
// Red wire to camera via opto-isolator.
#define PIN_CAMERA_POWER_ON 3
// Yellow wire to camera via opto-isolator.
#define PIN_CAMERA_SHUTTER 2
// A normally-open switch. When closed momentarily, the camera stays on until
// it is closed momentarily again.
#define PIN_CAMERA_STAY_ON_SWITCH 4
// Sense the memory-access LED on the camera to detect when an exposure is done
// and written to storage.
#define PIN_CAMERA_MEMORY_ACCESS A2

// Threshold for the analog reading for the camera memory-access LED. LED on
// (writing) is typically 0, and off (done) is typically 300.
#define CAMERA_ACCESS_THRESHOLD 50
// How long to wait between each time checking if the camera is done taking
// the exposure.
#define CAMERA_ACCESS_WAIT_INTERVAL_MS 50
// Number of times to repeat waiting for the camera to be done taking the
// exposure (1 minute).
#define CAMERA_FAILSAFE_WAIT_COUNT_LIMIT 1200

// A normally-open switch. When closed momentarily, go into fast mode (take
// pictures more often) for a while. If it's closed momentarily again, abort
// fast mode.
#define PIN_FAST_MODE_SWITCH 5
#define PHOTO_INTERVAL_FAST_SECONDS (10 * 60)
// When defined as (10 * 60 * 60) the exitFastModeSeconds calculation is
// incorrect.
#define FAST_MODE_DURATION_SECONDS 36000

#define PIN_DIVIDED_VCC A0
#define PIN_DIVIDED_PV  A1
// Resistor values for battery and photovoltaics. Each pair may be scaled
// together arbitrarily (ex: 4.7 and 1.2 or 47 and 12).
// The voltage dividers draw about 0.02mA each.
#define VCC_DIVIDER_SRC (1.000 + 0.992)
#define VCC_DIVIDER_GND 0.272
#define PV_DIVIDER_SRC  (0.997 + 0.996)
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
DHT22 dht(PIN_DHT22);
DHT22_ERROR_t dhtError;

uint32_t nextPhotoSeconds;
uint32_t exitFastModeSeconds;
bool isFastMode;
// In schedule mode, take photos at three predefined times during the day.
// Repeat the first hour +24 as the last value so there's always an hour in this
// list greater than the current hour, for ease of calculating a time in the
// future. These must have intervals of >1h.
uint8_t scheduleModeHours[] = {
    4,   // midnight
    13,  // mid-morning: 8AM (winter) or 9AM (summer)
    21,  // late afternoon but always before sunset: 4 (winter) or 5PM (summer)
    28
};

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
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);

  Wire.begin();
  rtc.begin();
  //setTime();
  setUpCameraPins();

  readTime();

  // Effectively start in slow mode and do not immediately take an exposure.
  isFastMode = true;
  nextPhotoSeconds = exitFastModeSeconds = sensorData.now.unixtime();
  scheduleNextPhotoGetIsTimeForPhoto();

  if (!sd.begin(PIN_SPI_CHIP_SELECT_REQUIRED, SPI_QUARTER_SPEED)) {
    sd.initErrorHalt();
  }
  digitalWrite(PIN_STATUS_LED, LOW);
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
  pinMode(PIN_FAST_MODE_SWITCH, INPUT_PULLUP);
}

void loop() {
  lowPowerSleepMillis(SLEEP_INTERVAL_MILLIS);

  readTime();
  readTemperatureAndHumidity();
  readVoltages();
  printData();
  writeData();
  if (scheduleNextPhotoGetIsTimeForPhoto()) {
    triggerCamera();
  }
  if (digitalRead(PIN_CAMERA_STAY_ON_SWITCH) == LOW) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
    delay(100);
    digitalWrite(PIN_CAMERA_POWER_ON, HIGH);
    delay(100);
    while (true) {
      Serial.println(F("Waiting on."));
      delay(1000);
      if (digitalRead(PIN_CAMERA_STAY_ON_SWITCH) == LOW) {
        Serial.println(F(" done"));
        break;
      }
    }
    delay(100);
    digitalWrite(PIN_CAMERA_POWER_ON, LOW);
    delay(1000);
    digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
    digitalWrite(PIN_STATUS_LED, LOW);
  }
}

bool scheduleNextPhotoGetIsTimeForPhoto() {
  uint32_t t = sensorData.now.unixtime();

  // Toggle modes.
  bool toggleFastMode =
      digitalRead(PIN_FAST_MODE_SWITCH) == LOW ||
      (isFastMode && (t >= exitFastModeSeconds));
  if (toggleFastMode) {
    isFastMode = !isFastMode;
    if (isFastMode) {
      Serial.print(F("+fast mode, ends: "));
      exitFastModeSeconds = t + FAST_MODE_DURATION_SECONDS;
      printDateTime(DateTime(exitFastModeSeconds));
      Serial.println();
      nextPhotoSeconds = t - 1;
      for (int i = 0; i < 20; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(50);
        digitalWrite(PIN_STATUS_LED, LOW);
        delay(50);
      }
    } else {
      Serial.println(F("+schedule mode."));
      for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(1000);
        digitalWrite(PIN_STATUS_LED, LOW);
        delay(100);
      }
    }
  }

  bool isTimeForPhoto = t >= nextPhotoSeconds;
  // Schedule the next photo.
  if (isFastMode) {
    while (nextPhotoSeconds <= t) {
      nextPhotoSeconds += PHOTO_INTERVAL_FAST_SECONDS;
    }
  } else if (isTimeForPhoto || toggleFastMode) {
    int i;
    uint8_t currentHour = sensorData.now.hour();
    while (scheduleModeHours[i] <= currentHour) {
      i++;
    }
    // DateTime::unixtime implicitly allows hours > 24 (no bounds checking).
    DateTime nextPhotoDateTime(
        sensorData.now.year(),
        sensorData.now.month(),
        sensorData.now.day(),
        scheduleModeHours[i]);
    nextPhotoSeconds = nextPhotoDateTime.unixtime();
    Serial.print(F("Next: "));
    printDateTime(nextPhotoDateTime);
    Serial.println();
  }

  return isTimeForPhoto;
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

void printDateTime(const DateTime& t) {
  Serial.print(t.unixtime());
  Serial.print(F(" "));
  Serial.print(t.year(), DEC);
  Serial.print('/');
  Serial.print(t.month(), DEC);
  Serial.print('/');
  Serial.print(t.day(), DEC);
  Serial.print(' ');
  Serial.print(t.hour(), DEC);
  Serial.print(':');
  Serial.print(t.minute(), DEC);
  Serial.print(':');
  Serial.print(t.second(), DEC);
}

void printData() {
  Serial.print(F("t: "));
  printDateTime(sensorData.now);

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
  // Wait for camera initialization.
  waitForCameraMemoryAccess();

  Serial.println(F(" shutter"));
  digitalWrite(PIN_CAMERA_SHUTTER, HIGH);
  delay(50);
  digitalWrite(PIN_CAMERA_SHUTTER, LOW);
  waitForCameraMemoryAccess();

  Serial.println(F(" power off"));
  digitalWrite(PIN_CAMERA_POWER_ON, LOW);
  delay(100);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
}

void waitForCameraMemoryAccess() {
  int a;
  int failsafeCount = 0;
  // Wait for the exposure to finish and the camera to start writing.
  Serial.println(F(" access start wait"));
  while (((a = analogRead(PIN_CAMERA_MEMORY_ACCESS)) < CAMERA_ACCESS_THRESHOLD)
      && failsafeCount < CAMERA_FAILSAFE_WAIT_COUNT_LIMIT) {
    delay(CAMERA_ACCESS_WAIT_INTERVAL_MS);
    failsafeCount++;
  }
  // Wait for for the camera to finish writing the image.
  Serial.println(F(" access end wait"));
  while (((a = analogRead(PIN_CAMERA_MEMORY_ACCESS)) > CAMERA_ACCESS_THRESHOLD)
      && failsafeCount < CAMERA_FAILSAFE_WAIT_COUNT_LIMIT) {
    delay(CAMERA_ACCESS_WAIT_INTERVAL_MS);
    failsafeCount++;
  }
  if (failsafeCount >= CAMERA_FAILSAFE_WAIT_COUNT_LIMIT) {
    Serial.println(F("memory access wait timeout"));
  }
}

void lowPowerSleepMillis(int millis) {
  // Finish sending any debug messages before disabling timers.
  Serial.flush();

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
