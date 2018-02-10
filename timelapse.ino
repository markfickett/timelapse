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

// pin numbers and timings
#include "config.h"

// Chronodot https://www.adafruit.com/products/255 and guide
// learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/wiring-it-up
// It draws 0.1mA consistently while 3.3 vcc is supplied.
RTC_DS1307 rtc;

// learn.adafruit.com/adafruit-led-backpack/0-dot-56-seven-segment-backpack
Adafruit_7segment display = Adafruit_7segment();

volatile unsigned long buttonPressTimesMillis[2] = {0L, 0L};
volatile boolean buttonPressed[2] = {false, false};

void handleWakePressed() { handleButtonPress(0); }
bool consumeWakePressed() { return consumeButtonPress(0); }
void handleChangePressed() { handleButtonPress(1); }
bool consumeChangePressed() { return consumeButtonPress(1); }
void handleButtonPress(int i) {
  unsigned long t = millis();
  if (!buttonPressed[i] &&
      t - buttonPressTimesMillis[i] > BUTTON_DEBOUNCE_MILLIS) {
    buttonPressed[i] = true;
    buttonPressTimesMillis[i] = t;
  }
}
bool consumeButtonPress(int i) {
  if (buttonPressed[i]) {
    buttonPressed[i] = false;
    return true;
  }
  return false;
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
  display.writeDigitRaw(0, 0b01110100); // h
  display.writeDigitRaw(1, 0b01111001); // E
  display.writeDigitRaw(3, 0b00111000); // L
  display.writeDigitRaw(4, 0b10111111); // O.
  display.writeDisplay();
  delay(1000);

  attachInterrupt(
      digitalPinToInterrupt(PIN_WAKE_BUTTON), handleWakePressed, FALLING);
  attachInterrupt(
      digitalPinToInterrupt(PIN_CHANGE_BUTTON), handleChangePressed, FALLING);
}

void setTime() {
  DateTime now = DateTime(__DATE__, __TIME__); // time at compilation
  now = now + TimeSpan(0, 5, 0, 0); // EDT => UTC
  rtc.adjust(now);
}

bool colonBlink = false;
void loop() {
  DateTime now = rtc.now();  // now.year(), .hour(), etc
  display.clear();
  display.println(now.hour() * 100 + now.minute());
  display.drawColon(colonBlink = !colonBlink);
  display.writeDisplay();
  delay(1000);

  /*
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, HIGH);
  delay(1000);
  digitalWrite(PIN_CAMERA_POWER_SUPPLY, LOW);
  delay(2000);
  */

  if (consumeWakePressed()) {
    display.clear();
    display.print(1337);
    display.writeDisplay();
    delay(1000);
  }
  if (consumeChangePressed()) {
    display.clear();
    display.writeDigitRaw(0, 0b00111001); // C
    display.writeDigitRaw(1, 0b01110100); // h
    display.writeDigitRaw(3, 0b11011100); // a
    display.writeDigitRaw(4, 0b01101111); // g
    display.writeDisplay();
    delay(1000);
  }

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

  display.clear();
  // 500 = normal room, 900 = dim, 950 = dark
  display.print(analogRead(PIN_AMBIENT_LIGHT_SENSE));
  display.writeDisplay();
  delay(1000);

  /*
  digitalWrite(PIN_CAMERA_EXPOSE, HIGH);
  digitalWrite(PIN_CAMERA_EXPOSE, LOW);
  */

  /*
  display.clear();
  display.print(analogRead(PIN_BATTERY_SENSE));
  display.writeDisplay();
  delay(1000);
  */

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
