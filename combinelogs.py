#!/usr/bin/env python
import csv
import datetime
import os

def FormatEpochSeconds(seconds_str):
  d = datetime.datetime.utcfromtimestamp(int(seconds_str))
  d += datetime.timedelta(hours=-4)  # UTC => Eastern
  return d.isoformat()

def CalculateBatteryVoltage(analog_reading_str):
  r1 = 4.689
  r2 = 1.190
  return (3.3 * int(analog_reading_str) * (r1 + r2)) / (1023 * r2)

with open('combined.csv', 'w') as out_file:
  out_csv = csv.writer(out_file)
  out_csv.writerow(['Date', 'Temp C', 'Humidity %','Supply V'])
  for log_filename in os.listdir('.'):
    if not log_filename.endswith('.CSV'):
      continue
    with open(log_filename) as log_file:
      log_csv = csv.reader(log_file)
      for row in log_csv:
        row[0] = FormatEpochSeconds(row[0])
        row[3] = CalculateBatteryVoltage(row[3])
        out_csv.writerow(row)
