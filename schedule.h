/**
 * Timing for when to take photos.
 */

#include "config.h"

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
     * Set semi-bogus defaults; expect real initialization after RTC is
     * initialized. TODO Save/restore with EEPROM?
     */
    Schedule() {
      setPeriodAndNextAfter(DateTime(), ONE_DAY);
    }

    bool isTimeFor(DateTime now) {
      return now.secondstime() >= next.secondstime();
    }

    void cyclePeriodAndRescheduleAfter(DateTime now) {
      setPeriodAndNextAfter(now, (period + 1) % SCHEDULE_PERIOD_SENTINEL);
    }

    void setPeriodAndNextAfter(DateTime now, SchedulePeriod newPeriod) {
      period = newPeriod;
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
};
