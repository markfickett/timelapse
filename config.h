// Pin-number and behavior configuration.

// Not the onboard status LED, which is used by the micro-SD card and obscured
// in this application.
#define PIN_STATUS_LED 6

// DHT-22 Temperature/humidity sensor: https://learn.adafruit.com/dht
// Draws .05mA between reads, 1.66mA while reading.
#define PIN_DHT22  8

// Increase this interval if DHT22 says DHT_ERROR_TOOQUICK (7). 5s is OK.
// This value is passed to Narcoleptic as an int, so must be < INT_MAX (32767).
#define SLEEP_INTERVAL_MILLIS (30 * 1000)

// Whether to read voltage of the solar panel. Turn this off if the charge
// controller does not tie the panel's ground to battery/load ground.
//#define READ_PV_VOLTAGE

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

// Whether to use fast mode (frequent photos whenever it's light), or schedule
// mode (photos at specific times).
#define FAST_MODE
#define PHOTO_INTERVAL_FAST_SECONDS (10 * 60)
// Only take frequent photos when the solar module is providing some voltage
// (as a proxy for only taking daytime photos) and when the battery has enough
// charge.
#ifdef READ_PV_VOLTAGE
#define FAST_MODE_PV_THRESHOLD_V 10.0
#else
#define FAST_MODE_PV_THRESHOLD_V -1
#endif
// 11.8v is generally considered a lower limit for safe SLA discharge. Allow a
// little buffer, since we want to keep logging even if battery voltage is too
// low to take photos. solarnavigator.net/battery_charging.htm
#define FAST_MODE_VCC_THRESHOLD_V 12.0
// Do not take frequent pictures at night. Night is any hour (UTC) between the
// two specified, excluding both. So with 0 and 8 (8PM and 4AM EDT),
//   0:44 is day
//   1:00 is night
//   7:58 is night
//   8:00 is day
// Values based on summer solstice for the lat/lng of interest.
// http://aa.usno.navy.mil/data/docs/RS_OneDay.php
#define FAST_MODE_NIGHT_AFTER_HOUR 0
#define FAST_MODE_NIGHT_BEFORE_HOUR 8

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

