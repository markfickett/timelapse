/**
 * Timing for when to take photos.
 */

#include <EEPROM.h>
#include "config.h"

#define SCHEDULE_EEPROM_MAGIC_NUMBER 0b01011101
#define SCHEDULE_EEPROM_ADDRESS_MN 0
#define SCHEDULE_EEPROM_ADDRESS_PERIOD 1

enum SchedulePeriod {
  ONE_MINUTE,
  TEN_MINUTES,
  ONE_HOUR,
  ONE_DAY,
  SCHEDULE_PERIOD_SENTINEL};

class Schedule {
  public:
    DateTime next;
    SchedulePeriod period;
    TimeSpan periodSpan;

    /**
     * Read saved SchedulePeriod from EEPROM.
     */
    Schedule(DateTime initialTimestamp) {
      Schedule::setUpEeprom();
      setPeriodAndNextAfter(initialTimestamp, Schedule::readFromEeprom());
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

  private:
    static void setUpEeprom() {
      if (EEPROM.read(SCHEDULE_EEPROM_ADDRESS_MN)
          == SCHEDULE_EEPROM_MAGIC_NUMBER) {
        return;
      }
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_PERIOD, TEN_MINUTES);
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_MN, SCHEDULE_EEPROM_MAGIC_NUMBER);
    }

    static SchedulePeriod readFromEeprom() {
      return EEPROM.read(SCHEDULE_EEPROM_ADDRESS_PERIOD);
    }

    static void writeToEeprom(SchedulePeriod periodToSave) {
      EEPROM.write(SCHEDULE_EEPROM_ADDRESS_PERIOD, periodToSave);
    }
};
