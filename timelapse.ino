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

// Chronodot https://www.adafruit.com/products/255 and guide
// learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/wiring-it-up
// It draws 0.1mA consistently while 3.3 vcc is supplied.
RTC_DS1307 rtc;

// learn.adafruit.com/adafruit-led-backpack/0-dot-56-seven-segment-backpack
Adafruit_7segment display = Adafruit_7segment();

byte charTo7Seg(char c) {
  switch(c) {
    case 'C': return 0b00111001;
    case 'E': return 0b01111001;
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
    case 'g': return 0b01101111;
    case 'h': return 0b01110100;
    case 'n': return 0b01010100;
    case 'y': return 0b01101110;
    case '.': return 0b10000000;
    case '!': return 0b10000010;
    default:
      return 0b11111111;
  }
}

void setup() {
  Serial.begin(57600);
  // This must be called before analogRead if voltage is applied to the AREF
  // pin, or else the board may be damaged.
  analogReference(EXTERNAL);

  rtc.begin();
  //setTime();

  pinMode(PIN_CAMERA_ACTIVITY_SENSE, INPUT);
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
  display.begin(0x70 /* default I2C address */); // also sets max brightness
  display.clear();
  display.writeDigitRaw(0, charTo7Seg('h'));
  display.writeDigitRaw(1, charTo7Seg('E'));
  display.writeDigitRaw(3, charTo7Seg('L'));
  display.writeDigitRaw(4, charTo7Seg('O') | charTo7Seg('.'));
  display.writeDisplay();
  delay(1000);

  attachInterrupt(
      digitalPinToInterrupt(PIN_WAKE_BUTTON), handleWakePress, FALLING);
  attachInterrupt(
      digitalPinToInterrupt(PIN_CHANGE_BUTTON), handleChangePress, FALLING);
}

void setTime() {
  DateTime now = DateTime(__DATE__, __TIME__); // time at compilation
  now = now + TimeSpan(0, 5, 0, 0); // EDT => UTC
  rtc.adjust(now);
}

struct EnvironmentData {
  DateTime now;

  int ambientLightLevel; // raw analogRead value
  bool ambientIsLight;

  int vccVoltageReading; // raw analogRead value
  float vccVoltage;
};

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
      * (VCC_DIVIDER_SRC + VCC_DIVIDER_GND) / VCC_DIVIDER_GND;
  return data;
}

void debugMode() {
  struct EnvironmentData data = getEnvironmentData();
  consumeChangePress();

  // time, UTC
  bool colonBlink = false;
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitNum(0, data.now.hour() / 10);
    display.writeDigitNum(1, data.now.hour() % 10);
    display.drawColon(colonBlink = !colonBlink);
    display.writeDigitNum(3, data.now.minute() / 10);
    display.writeDigitNum(4, data.now.minute() % 10);
    display.writeDisplay();
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
      takePicture();
    }
    delay(50);
  }

  // ambient light level
  while (!consumeWakePress()) {
    display.clear();
    display.writeDigitRaw(0, charTo7Seg('L'));
    display.writeDigitRaw(1, charTo7Seg('I'));
    display.writeDigitRaw(3, charTo7Seg('g'));
    display.writeDigitRaw(4, charTo7Seg('h'));
    display.writeDisplay();
    delay(500);
    display.clear();
    if (data.ambientIsLight) {
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
    display.println(data.ambientLightLevel);
    display.writeDisplay();
    delay(1000);
    data = getEnvironmentData();
  }

  // TODO next photo time & interval

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
}

void checkConditionsMaybeTakePicture() {
  struct EnvironmentData data = getEnvironmentData();
  if (data.vccVoltage < VCC_VOLTAGE_LOW || !data.ambientIsLight) {
    return;
  }
  /* TODO
  if (it's time) {
    takePicture();
  }
  */
}

void takePicture() {
  display.clear();
  display.writeDigitRaw(0, charTo7Seg('E'));
  display.writeDigitRaw(1, charTo7Seg('X'));
  display.writeDigitRaw(3, charTo7Seg('P'));
  display.writeDisplay();

  delay(1000);
  /*
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
  delay(1000);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
  delay(2000);
  */

  /*
  digitalWrite(PIN_CAMERA_FOCUS, HIGH);
  delay(1000);
  digitalWrite(PIN_CAMERA_FOCUS, LOW);
  */

  /*
  display.clear();
  display.print(analogRead(PIN_CAMERA_ACTIVITY_SENSE));
  display.writeDisplay();
  delay(1000);
  */

  /*
  digitalWrite(PIN_CAMERA_EXPOSE, HIGH);
  digitalWrite(PIN_CAMERA_EXPOSE, LOW);
  */
  display.clear();
  display.writeDisplay();
}

void loop() {
  if (consumeWakePress()) {
    debugMode();
  }
  checkConditionsMaybeTakePicture();

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
