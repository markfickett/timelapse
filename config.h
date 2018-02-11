// Pin-number and behavior configuration.

// This value is passed to Narcoleptic as an int, so must be < INT_MAX (32767).
#define SLEEP_INTERVAL_MILLIS (10 * 1000)

// 12v (battery) power supply for the 9V regulator powering the camera.
#define PIN_CAMERA_POWER_SUPPLY 5

// To camera via opto-isolator.
#define PIN_CAMERA_FOCUS 6
#define PIN_CAMERA_EXPOSE 7
// Sense the memory-access LED on the camera to detect when an exposure is done
// and written to storage.
#define PIN_CAMERA_ACTIVITY_SENSE A1

// 500 = normal room, 900 = dim, 950+ = dark
#define PIN_AMBIENT_LIGHT_SENSE A2
#define AMBIENT_LIGHT_LEVEL_DARK 950

#define PIN_BATTERY_SENSE A3
#define AREF 5.0

// 11.8v is generally considered a lower limit for safe SLA discharge. Allow a
// little buffer, since we want to keep logging even if battery voltage is too
// low to take photos. solarnavigator.net/battery_charging.htm
#define VCC_VOLTAGE_LOW 11.8

// Resistor values for battery. Each pair may be scaled together arbitrarily
// (ex: 4.7 and 1.2 or 47 and 12).
#define VCC_DIVIDER_SRC 838
#define VCC_DIVIDER_GND 332
// Max tolerable Vcc = 17.6v =
//     AREF * (VCC_DIVIDER_SRC + VCC_DIVIDER_GND) / VCC_DIVIDER_GND

// Threshold for the analog reading for the camera memory-access LED. LED on
// (writing) is typically 0, and off (done) is typically 300.
#define CAMERA_ACCESS_THRESHOLD 50
// How long to wait between each time checking if the camera is done taking
// the exposure.
#define CAMERA_ACCESS_WAIT_INTERVAL_MS 50
// Number of times to repeat waiting for the camera to be done taking the
// exposure (1 minute).
#define CAMERA_FAILSAFE_WAIT_COUNT_LIMIT 1200

#define PIN_WAKE_BUTTON 2
#define PIN_CHANGE_BUTTON 3

#define PIN_7SEG_POWER 4

#define BUTTON_DEBOUNCE_MILLIS 500
