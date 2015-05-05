#!/usr/bin/env python
# Concatenate all CSVs in the logs directory and process raw voltage values.
import csv
import datetime
import os

LOGS_DIR = 'logs'
HEADERS = ['Date', 'Temp C', 'Humidity %','Supply V', 'Solar Panel V']
AREF = 3.3

def FormatEpochSeconds(seconds_str):
  d = datetime.datetime.utcfromtimestamp(int(seconds_str))
  d += datetime.timedelta(hours=-4)  # UTC => Eastern
  return d.isoformat()

def CalculateBatteryVoltage(analog_reading_str, source_r, gnd_r):
  return (AREF * int(analog_reading_str) * (source_r + gnd_r)) / (1023 * gnd_r)

out_filename = os.path.join(LOGS_DIR, 'combined.csv')

if os.path.isfile(out_filename):
  print 'overwriting', out_filename
  os.remove(out_filename)
input_filenames = os.listdir(LOGS_DIR)

row_num = 0
with open(out_filename, 'w') as out_file:
  out_csv = csv.writer(out_file)
  out_csv.writerow(HEADERS)
  for log_filename in input_filenames:
    if not log_filename.lower().endswith('.csv'):
      continue
    with open(os.path.join(LOGS_DIR, log_filename)) as log_file:
      log_csv = csv.reader(log_file)
      print 'processing', log_filename
      for row in log_csv:
        row[0] = FormatEpochSeconds(row[0])
        row[3] = CalculateBatteryVoltage(row[3], 1.000, 0.272)
        row[4] = CalculateBatteryVoltage(row[4], 0.997, 0.272)
        out_csv.writerow(row)
        row_num += 1

print 'wrote %d rows to %s' % (row_num, out_filename)
