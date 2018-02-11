/**
 * Interrupt-driven button press detection.
 */

#include "config.h"

volatile unsigned long buttonPressTimesMillis[2] = {0L, 0L};
volatile boolean buttonPress[2] = {false, false};

bool consumeButtonPress(int i) {
  if (buttonPress[i]) {
    buttonPress[i] = false;
    return true;
  }
  return false;
}

void handleButtonPress(int i) {
  unsigned long t = millis();
  if (!buttonPress[i] &&
      t - buttonPressTimesMillis[i] > BUTTON_DEBOUNCE_MILLIS) {
    buttonPress[i] = true;
    buttonPressTimesMillis[i] = t;
  }
}

void handleWakePress() { handleButtonPress(0); }
bool consumeWakePress() { return consumeButtonPress(0); }
void handleChangePress() { handleButtonPress(1); }
bool consumeChangePress() { return consumeButtonPress(1); }
