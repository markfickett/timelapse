/**
 * Timing for when to take photos.
 */

#include <EEPROM.h>
#include "config.h"

#define SCHEDULE_EEPROM_MAGIC_NUMBER 0b01011110
#define SCHEDULE_EEPROM_ADDRESS_MN 0
#define SCHEDULE_EEPROM_ADDRESS_PERIOD 1
#define SCHEDULE_EEPROM_ADDRESS_LAST_PHOTO 2

enum SchedulePeriod {
  ONE_MINUTE,
  TEN_MINUTES,
  ONE_HOUR,
  ONE_DAY,
  SCHEDULE_PERIOD_SENTINEL};

/**
 * Writes a 4 byte (32bit) long to the eeprom at the specified address
 * to address + 3.
 *
 * From https://playground.arduino.cc/Code/EEPROMReadWriteLong
 */
void EEPROMWritelong(int address, uint32_t value) {
  // Decomposition from a long to 4 bytes by using bitshift.
  // One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  // Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address) {
  // Read the 4 bytes from the EEPROM memory.
  uint32_t four = EEPROM.read(address);
  uint32_t three = EEPROM.read(address + 1);
  uint32_t two = EEPROM.read(address + 2);
  uint32_t one = EEPROM.read(address + 3);

  // Return the recomposed long by using bitshift.
  return
      ((four << 0) & 0xFF) +
      ((three << 8) & 0xFF00) +
      ((two << 16) & 0xFF0000) +
      ((one << 24) & 0xFF000000);
}

class Schedule {
  public:
    DateTime next;
    SchedulePeriod period;
    TimeSpan periodSpan;

    /**
     * Read saved SchedulePeriod from EEPROM.
     */
    Schedule() {
      Schedule::setUpEeprom();
      setPeriodAndNextAfter(
          Schedule::getLastPhotoTime(),
          Schedule::readFromEeprom());
    }

    bool isTimeFor(DateTime now) {
      return now.secondstime() >= next.secondstime();
    }

    void cyclePeriodAndRescheduleAfter(DateTime now) {
      setPeriodAndNextAfter(now, (period + 1) % SCHEDULE_PERIOD_SENTINEL);
    }

    void setPeriodAndNextAfter(DateTime now, SchedulePeriod newPeriod) {
      period = newPeriod;
      Schedule::writeToEeprom(period);
      switch(period) {
        case ONE_MINUTE:
          periodSpan = TimeSpan(0, 0, 1, 0);
          break;
        case ONE_HOUR:
          periodSpan = TimeSpan(0, 1, 0, 0);
          break;
        case ONE_DAY:
          periodSpan = TimeSpan(1, 0, 0, 0);
          break;
        case TEN_MINUTES:
        default:
          periodSpan = TimeSpan(0, 0, 10, 0);
          break;
      }
      advancePast(now);
    }

    void advancePast(DateTime now) {
      DateTime periodAligned;
      switch(period) {
        case ONE_MINUTE:
          periodAligned = DateTime(
              now.year(), now.month(), now.day(),
              now.hour(), now.minute(), 0);
          break;
        case TEN_MINUTES:
          periodAligned = DateTime(
              now.year(), now.month(), now.day(),
              now.hour(), 10 * (now.minute() / 10), 0);
          break;
        case ONE_HOUR:
          periodAligned = DateTime(
              now.year(), now.month(), now.day(),
              now.hour(), 0, 0);
          break;
        case ONE_DAY:
          periodAligned = DateTime(
              now.year(), now.month(), now.day(),
              DAILY_HOUR_OF_DAY_UTC, 0, 0);
          break;
      }
      while (periodAligned.secondstime() <= now.secondstime()) {
        periodAligned = periodAligned + periodSpan;
      }
      next = periodAligned;
    }

    void recordLastPhoto(DateTime now) {
      EEPROMWritelong(SCHEDULE_EEPROM_ADDRESS_LAST_PHOTO, now.unixtime());
    }

    DateTime getLastPhotoTime() {
      return DateTime(EEPROMReadlong(SCHEDULE_EEPROM_ADDRESS_LAST_PHOTO));
    }

  private:
    static void setUpEeprom() {
      if (EEPROM.read(SCHEDULE_EEPROM_ADDRESS_MN)
          == SCHEDULE_EEPROM_MAGIC_NUMBER) {
        return;
      }
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_PERIOD, TEN_MINUTES);
      EEPROM.write(
          SCHEDULE_EEPROM_ADDRESS_LAST_PHOTO, 0L /* some time in the past */);
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_MN, SCHEDULE_EEPROM_MAGIC_NUMBER);
    }

    static SchedulePeriod readFromEeprom() {
      return EEPROM.read(SCHEDULE_EEPROM_ADDRESS_PERIOD);
    }

    static void writeToEeprom(SchedulePeriod periodToSave) {
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_PERIOD, periodToSave);
    }
};
