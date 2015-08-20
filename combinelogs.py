#!/usr/bin/env python
# Concatenate all CSVs in the logs directory and process raw voltage values.
import csv
import datetime
import os
import sys

HEADERS = ['Date', 'Temp C', 'Humidity %','Supply V', 'Solar Panel V']
AREF = 2.048

def FormatEpochSeconds(seconds_str):
  d = datetime.datetime.utcfromtimestamp(int(seconds_str))
  d += datetime.timedelta(hours=-4)  # UTC => Eastern
  return d.isoformat()

def CalculateBatteryVoltage(analog_reading_str, source_r, gnd_r):
  return (AREF * int(analog_reading_str) * (source_r + gnd_r)) / (1023 * gnd_r)

if __name__ == '__main__':
  if len(sys.argv) != 2:
    print 'Usage: %s LOGS_DIR' % sys.argv[0]
    sys.exit(1)

  logs_dir = sys.argv[1]
  log_files = os.listdir(logs_dir)
  print 'reading from %s: %s' % (logs_dir, log_files)
  out_filename = os.path.join(logs_dir, 'combined.csv')

  if os.path.isfile(out_filename):
    print 'overwriting', out_filename
    os.remove(out_filename)
  input_filenames = os.listdir(logs_dir)

  row_num = 0
  with open(out_filename, 'w') as out_file:
    out_csv = csv.writer(out_file)
    out_csv.writerow(HEADERS)
    for log_filename in input_filenames:
      if not log_filename.lower().endswith('.csv'):
        continue
      with open(os.path.join(logs_dir, log_filename)) as log_file:
        log_csv = csv.reader(log_file)
        print 'processing', log_filename
        for i, row in enumerate(log_csv, 1):
          if len(row) != 5:
            raise ValueError(
                '%s:%d of wrong length %d should be 5: %r'
                % (log_filename, i, len(row), row))
          row[0] = FormatEpochSeconds(row[0])
          row[3] = CalculateBatteryVoltage(row[3], 1.000 + 0.992, 0.272)  # batt
          row[4] = CalculateBatteryVoltage(row[4], 0.997 + 0.996, 0.272)  # PV
          out_csv.writerow(row)
          row_num += 1

  print 'wrote %d rows to %s' % (row_num, out_filename)
