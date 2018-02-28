/**
 * Run time lapse camera.
 */

#include "Wire.h"
// Using https://github.com/adafruit/RTClib
#include "RTClib.h"
// Adafruit 7-segment LED
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
// https://github.com/rocketscream/Low-Power
#include "LowPower.h"

#include "config.h"
#include "buttons.h"
#include "schedule.h"

// Chronodot https://www.adafruit.com/products/255 and guide
// learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/wiring-it-up
// It draws 0.1mA consistently while 3.3 vcc is supplied.
RTC_DS1307 rtc;

// learn.adafruit.com/adafruit-led-backpack/0-dot-56-seven-segment-backpack
Adafruit_7segment display = Adafruit_7segment();

struct EnvironmentData {
  DateTime now;

  int ambientLightLevel; // raw analogRead value
  bool ambientIsLight;

  int vccVoltageReading; // raw analogRead value
  float vccVoltage;

  // from camera activity LED
  int cameraIdleReading; // raw analogRead value
  bool cameraIsIdle;
};

byte charTo7Seg(char c) {
  switch(c) {
    case 'A': return 0b01110111;
    case 'C': return 0b00111001;
    case 'E': return 0b01111001;
    case 'F': return 0b01110001;
    case 'X':
    case 'H': return 0b01110110;
    case 'I': return 0b00110000;
    case 'L': return 0b00111000;
    case 'O': return 0b00111111;
    case 'P': return 0b01110011;
    case 'S': return 0b01101101;
    case 'U': return 0b00111110;
    case 'V': return 0b00111100;
    case 'a': return 0b11011100;
    case 'b': return 0b01111100;
    case 'd': return 0b01011110;
    case 'g': return 0b01101111;
    case 'h': return 0b01110100;
    case 'n': return 0b01010100;
    case 't': return 0b01110000;
    case 'y': return 0b01101110;
    case '.': return 0b10000000;
    case '!': return 0b10000010;
    default:
      return 0b11111111;
  }
}

Schedule schedule;

void setup() {
  // This must be called before analogRead if voltage is applied to the AREF
  // pin, or else the board may be damaged.
  analogReference(EXTERNAL);

  pinMode(PIN_CAMERA_IDLE_SENSE, INPUT);
  pinMode(PIN_AMBIENT_LIGHT_SENSE, INPUT);
  pinMode(PIN_BATTERY_SENSE, INPUT);

  pinMode(PIN_CAMERA_POWER_SUPPLY, OUTPUT);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);

  pinMode(PIN_WAKE_BUTTON, INPUT_PULLUP);
  pinMode(PIN_CHANGE_BUTTON, INPUT_PULLUP);

  pinMode(PIN_CAMERA_FOCUS, OUTPUT);
  digitalWrite(PIN_CAMERA_FOCUS, LOW);
  pinMode(PIN_CAMERA_EXPOSE, OUTPUT);
  digitalWrite(PIN_CAMERA_EXPOSE, LOW);

  pinMode(PIN_7SEG_POWER, OUTPUT);
  digitalWrite(PIN_7SEG_POWER, HIGH);
  delay(50);  // Give it time to wake up.
  display.begin(0x70 /* default I2C address */); // also sets max brightness
  display.clear();
  display.writeDigitRaw(0, charTo7Seg('h'));
  display.writeDigitRaw(1, charTo7Seg('E'));
  display.writeDigitRaw(3, charTo7Seg('L'));
  display.writeDigitRaw(4, charTo7Seg('O') | charTo7Seg('.'));
  display.writeDisplay();
  delay(1000);

  // Real-time clock setup.
  rtc.begin();
  int clockProgress = 0;
  while (!rtc.isrunning()) {
    // It may be necessary to wait, after begin(), before adjust() can be
    // called. Docs on isrunning() are unclear whether it's really the right
    // check.
    display.clear();
    clockProgress = clockProgress + 1 % 5;
    display.writeDigitRaw(clockProgress, charTo7Seg('.'));
    display.writeDisplay();
    delay(100);
  }
  // If the clock's date is set way in the past, it is probably fresh or reset
  // and should be set to the host machine's clock at compile time.
  if (rtc.now().year() < 2018) {
    DateTime now = DateTime(__DATE__, __TIME__); // time at compilation
    now = now + TimeSpan(0, 5, 0, 0); // EDT => UTC
    rtc.adjust(now);
  }

  attachInterrupt(
      digitalPinToInterrupt(PIN_WAKE_BUTTON), handleWakePress, FALLING);
  attachInterrupt(
      digitalPinToInterrupt(PIN_CHANGE_BUTTON), handleChangePress, FALLING);
}

struct EnvironmentData getEnvironmentData() {
  struct EnvironmentData data;
  data.now = rtc.now();
  data.ambientLightLevel = analogRead(PIN_AMBIENT_LIGHT_SENSE);
  data.ambientIsLight = data.ambientLightLevel < AMBIENT_LIGHT_LEVEL_DARK;
  data.vccVoltageReading = analogRead(PIN_BATTERY_SENSE);
  data.vccVoltage =
      // Convert reading to sensed voltage.
      (data.vccVoltageReading / 1023.0) * AREF
      // Scale sensed voltage to actual Vcc based on dividers.
      * VCC_DIVIDER_ADJ * (VCC_DIVIDER_SRC + VCC_DIVIDER_GND) / VCC_DIVIDER_GND;
  data.cameraIdleReading = analogRead(PIN_CAMERA_IDLE_SENSE);
  data.cameraIsIdle = data.cameraIdleReading >= CAMERA_IDLE_THRESHOLD;
  return data;
}

void displayTime(int hours, int minutes, bool drawColon) {
  display.clear();
  display.writeDigitNum(0, hours / 10);
  display.writeDigitNum(1, hours % 10);
  display.drawColon(drawColon);
  display.writeDigitNum(3, minutes / 10);
  display.writeDigitNum(4, minutes % 10);
  display.writeDisplay();
}

void debugMode() {
  struct EnvironmentData data = getEnvironmentData();
  consumeChangePress();  // clear any old presses

  // Schedule / next photo
  while (!consumeWakePress()) {
    data = getEnvironmentData();
    TimeSpan remaining = schedule.next - data.now;

    // current interval
    display.clear();
    switch(schedule.period) {
      case ONE_MINUTE: // :01
        display.drawColon(true);
        display.writeDigitNum(3, 0);
        display.writeDigitNum(4, 1);
        break;
      case TEN_MINUTES: // :10
        display.writeDigitNum(3, 1);
        display.writeDigitNum(4, 0);
        display.drawColon(true);
        break;
      case ONE_HOUR: // 1h
        display.writeDigitNum(0, 1);
        display.writeDigitRaw(1, charTo7Seg('h'));
        break;
      case ONE_DAY: // 1d
        display.writeDigitNum(0, 1);
        display.writeDigitRaw(1, charTo7Seg('d'));
        break;
    }
    display.writeDisplay();
    delay(500);

    // Is it still SCHEduled for the future, or are we PAST the schedule time?
    display.clear();
    if (remaining.totalseconds() >= 0) {
      display.writeDigitRaw(0, charTo7Seg('S'));
      display.writeDigitRaw(1, charTo7Seg('C'));
      display.writeDigitRaw(3, charTo7Seg('H'));
      display.writeDigitRaw(4, charTo7Seg('E'));
    } else {
      display.writeDigitRaw(0, charTo7Seg('P'));
      display.writeDigitRaw(1, charTo7Seg('A'));
      display.writeDigitRaw(3, charTo7Seg('S'));
      display.writeDigitRaw(4, charTo7Seg('t'));
    }
    display.writeDisplay();
    delay(500);

    // time remaining/elapsed (absolute value)
    if (remaining.totalseconds() < 0) {
      remaining = TimeSpan(-remaining.totalseconds());
    }
    display.clear();
    if (remaining.totalseconds() <= 60) {
      display.println(remaining.totalseconds());
    } else if (remaining.days() <= 2) {
      displayTime(
          remaining.hours() + remaining.days() * 24,
          remaining.minutes(),
          true /* drawColon */);
    } else {
      display.println(remaining.days() * 10);
      display.writeDigitRaw(4, charTo7Seg('d'));
    }
    display.writeDisplay();
    delay(1000);

    if (consumeChangePress()) {
      schedule.cyclePeriodAndRescheduleAfter(schedule.getLastPhotoTime());
    }
  }

  // time, UTC
  bool colonBlink = false;
  while (!consumeWakePress()) {
    displayTime(data.now.hour(), data.now.minute(), colonBlink = !colonBlink);
    delay(1000);
    data = getEnvironmentData();
  }

  // VCC (battery voltage)
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitRaw(0, charTo7Seg('V'));
    display.writeDigitRaw(1, charTo7Seg('C'));
    display.writeDigitRaw(3, charTo7Seg('C'));
    display.writeDisplay();
    delay(500);
    display.clear();
    display.println(data.vccVoltage);
    display.writeDisplay();
    delay(1000);
    display.clear();
    display.println(data.vccVoltageReading);
    display.writeDisplay();
    delay(1000);
    data = getEnvironmentData();
  }

  // take a picture
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitRaw(0, charTo7Seg('P'));
    display.writeDigitRaw(1, charTo7Seg('I'));
    display.writeDigitRaw(3, charTo7Seg('C'));
    display.writeDisplay();
    if (consumeChangePress()) {
      schedule.recordLastPhoto(data.now);
      takePicture();
      schedule.advancePast(data.now);
    }
    delay(50);
  }

  // camera power on/off
  bool isOn = false;
  while (!consumeWakePress()) {
    display.clear();
    if (isOn) {
      display.writeDigitRaw(0, charTo7Seg('O'));
      display.writeDigitRaw(1, charTo7Seg('n'));
    } else {
      display.writeDigitRaw(0, charTo7Seg('O'));
      display.writeDigitRaw(1, charTo7Seg('F'));
      display.writeDigitRaw(3, charTo7Seg('F'));
    }
    display.writeDisplay();
    if (consumeChangePress()) {
      isOn = !isOn;
      digitalWrite(PIN_CAMERA_POWER_SUPPLY, isOn ? HIGH : LOW);
    }
    delay(50);
  }
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);

  // ambient light level
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitRaw(0, charTo7Seg('L'));
    display.writeDigitRaw(1, charTo7Seg('I'));
    display.writeDigitRaw(3, charTo7Seg('g'));
    display.writeDigitRaw(4, charTo7Seg('h'));
    display.writeDisplay();
    delay(500);
    displayThreshold(data.ambientIsLight, data.ambientLightLevel);
    data = getEnvironmentData();
  }

  // camera activity LED
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitRaw(0, charTo7Seg('I'));
    display.writeDigitRaw(1, charTo7Seg('d'));
    display.writeDigitRaw(3, charTo7Seg('L'));
    display.writeDigitRaw(4, charTo7Seg('E'));
    display.writeDisplay();
    delay(500);
    displayThreshold(data.cameraIsIdle, data.cameraIdleReading);
    data = getEnvironmentData();
  }

  display.clear();
  display.writeDigitRaw(0, charTo7Seg('b'));
  display.writeDigitRaw(1, charTo7Seg('y'));
  display.writeDigitRaw(3, charTo7Seg('E'));
  display.writeDigitRaw(4, charTo7Seg('!'));
  display.writeDisplay();
  delay(500);
  for(int brightness = 15; brightness >= 0; brightness--) {
    delay(100);
    display.setBrightness(brightness);
  }
  display.clear();
  display.writeDisplay();
  display.setBrightness(15);
  consumeWakePress();  // clear any stray presses again
  consumeChangePress();
}

void displayThreshold(bool meetsThreshold, int analogValue) {
  display.clear();
  if (meetsThreshold) {
    display.writeDigitRaw(0, charTo7Seg('y'));
    display.writeDigitRaw(1, charTo7Seg('E'));
    display.writeDigitRaw(3, charTo7Seg('S'));
  } else {
    display.writeDigitRaw(1, charTo7Seg('n'));
    display.writeDigitRaw(3, charTo7Seg('O'));
  }
  display.writeDisplay();
  delay(500);
  display.clear();
  display.println(analogValue);
  display.writeDisplay();
  delay(1000);
}

void takePicture() {
  display.clear();
  display.writeDigitRaw(0, charTo7Seg('E'));
  display.writeDigitRaw(1, charTo7Seg('X'));
  display.writeDigitRaw(3, charTo7Seg('P'));
  display.writeDisplay();

  digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
  waitForCameraActivity();

  digitalWrite(PIN_CAMERA_FOCUS, HIGH);
  digitalWrite(PIN_CAMERA_EXPOSE, HIGH);
  delay(CAMERA_EXPOSE_TIME_MS);
  digitalWrite(PIN_CAMERA_FOCUS, LOW);
  digitalWrite(PIN_CAMERA_EXPOSE, LOW);

  waitForCameraActivity();

  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
  display.clear();
  display.writeDisplay();
}

void waitForCameraActivity() {
  struct EnvironmentData data = getEnvironmentData();
  int checkCount = 0;
  // Wait for the activity light to come on...
  while (data.cameraIsIdle && checkCount++ < CAMERA_FAILSAFE_WAIT_COUNT_LIMIT) {
    delay(CAMERA_ACCESS_WAIT_INTERVAL_MS);
    data = getEnvironmentData();
  }
  checkCount = 0;
  int idleCycles = 0;
  // ...and then to stay off.
  while (idleCycles < 4 && checkCount++ < CAMERA_FAILSAFE_WAIT_COUNT_LIMIT) {
    if (data.cameraIsIdle) {
      idleCycles++;
    }
    delay(CAMERA_ACCESS_WAIT_INTERVAL_MS);
    data = getEnvironmentData();
  }
}

void loop() {
  if (consumeWakePress()) {
    debugMode();
  }

  struct EnvironmentData data = getEnvironmentData();
  if (data.vccVoltage >= VCC_VOLTAGE_LOW
      && data.ambientIsLight
      && schedule.isTimeFor(data.now)) {
    // Record the last photo time before taking it in case of brown-outs.
    schedule.recordLastPhoto(data.now);
    takePicture();
    schedule.advancePast(data.now);
  }

  lowPowerSleep();
}

void lowPowerSleep() {
  display.clear();
  display.writeDisplay();
  display.setBrightness(0);
  digitalWrite(PIN_7SEG_POWER, LOW);

  // During power-down, the "wake" button interrupt will be registered, and will
  // immediately cancel power-down mode.
  // With adafru.it/1065 step-down transformer (6.5-32v in, 5v out), the board
  // draws 1.3mA asleep. (The power regulator draws 1-2mA, so accounts for most
  // if not all of this.)
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON);

  digitalWrite(PIN_7SEG_POWER, HIGH);
  // Give the display time to wake up. Without an explicit delay, it takes
  // 2-4s to reconnect [retries/backoff?].
  delay(50);
  // Must call begin() again after power cycling the display.
  display.begin(0x70 /* default I2C address */); // also sets max brightness
}
