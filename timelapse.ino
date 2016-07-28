/**
 * Run time lapse camera, monitoring environment and battery stats.
 *
 * Overall, the system draws (@12v) 0.7mA while idle, 14mA during data
 * sampling/writes, and 200mA-2A when triggering the camera.
 */

#include "Wire.h"
// Using https://github.com/ringerc/Arduino-DHT22 for Pro Micro 3.3v .
// Turning off timers for sleep requires replacing millis() in DHT22.cpp with
// Narcoleptic.millis().
#include "DHT22.h"
// Using https://github.com/adafruit/RTClib
#include "RTClib.h"
// Using https://github.com/greiman/SdFat
#include "SdFat.h"
// Using https://code.google.com/p/narcoleptic/source/browse/
#include "Narcoleptic.h"

#include "config.h"

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
// It draws 0.1mA consistently while 3.3 vcc is supplied.
RTC_DS1307 rtc;

char buf[128]; // filenames and data log lines
SdFat sd;
boolean sdError;
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

  // Schedule the next photo in the past, for the most recent 10-minute
  // increment. This way a photo will be taken immediately, and subsequent
  // photos will be taken at :00, :10, :20, etc.
  nextPhotoSeconds = sensorData.now.unixtime();
  nextPhotoSeconds -= nextPhotoSeconds % PHOTO_INTERVAL_FAST_SECONDS;

  sdError = false;
  if (!sd.begin(PIN_SPI_CHIP_SELECT_REQUIRED, SPI_QUARTER_SPEED)) {
    sdError = true;
    sd.initErrorPrint();
    for (int i = 0; i < 10; i++) {
      digitalWrite(PIN_STATUS_LED, HIGH);
      delay(50);
      digitalWrite(PIN_STATUS_LED, LOW);
      delay(100);
    }
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
}

void loop() {
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

  lowPowerSleepMillis(SLEEP_INTERVAL_MILLIS);
}

bool scheduleNextPhotoGetIsTimeForPhoto() {
  uint32_t t = sensorData.now.unixtime();
  bool isTimeForPhoto;

#ifdef FAST_MODE
  if (t < nextPhotoSeconds) {
    isTimeForPhoto = false;
  } else if (sensorData.now.hour() > FAST_MODE_NIGHT_AFTER_HOUR &&
      sensorData.now.hour() < FAST_MODE_NIGHT_BEFORE_HOUR) {
    Serial.print("Skipping night photo at UTC hour ");
    Serial.println(sensorData.now.hour());
    isTimeForPhoto = false;
  } else {
    Serial.println("Time for photo.");
    int pvReading = sensorData.dividedPhotoVoltaic;
    float pvVoltage =
        (pvReading * AREF * (PV_DIVIDER_SRC + PV_DIVIDER_GND))
        / (1023 * PV_DIVIDER_GND);
    int vccReading = sensorData.dividedVcc;
    float vccVoltage =
        (vccReading * AREF * (VCC_DIVIDER_SRC + VCC_DIVIDER_GND))
        / (1023 * VCC_DIVIDER_GND);
    if (pvVoltage < FAST_MODE_PV_THRESHOLD_V) {
      Serial.print("PV voltage ");
      Serial.print(pvVoltage);
      Serial.println("v too low, skipping photo.");
      isTimeForPhoto = false;
    } else if (vccVoltage < FAST_MODE_VCC_THRESHOLD_V) {
      Serial.print("VCC voltage ");
      Serial.print(vccVoltage);
      Serial.println("v too low, skipping photo.");
      isTimeForPhoto = false;
    } else {
      isTimeForPhoto = true;
    }
  }
  while (nextPhotoSeconds <= t) {
    nextPhotoSeconds += PHOTO_INTERVAL_FAST_SECONDS;
  }
#else
  isTimeForPhoto = t >= nextPhotoSeconds;
  // Find the next scheduled photo time.
  if (isTimeForPhoto) {
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
#endif

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
#ifdef READ_PV_VOLTAGE
  sensorData.dividedPhotoVoltaic = analogRead(PIN_DIVIDED_PV);
#else
  sensorData.dividedPhotoVoltaic = 0;
#endif
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
  if (!sdError && !logFile.open(buf, O_CREAT | O_WRITE | O_AT_END)) {
    sdError = true;
    sd.errorPrint("Opening logFile failed. ");
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

  if (!sdError && !logFile.sync() || logFile.getWriteError()) {
    sdError = true;
    sd.errorPrint("Writing logFile failed. ");
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
