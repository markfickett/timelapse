logging_total = [0, 0]
exposure_total = [0, 0]
is_header = True
last_ms = None
with open('powerusage/powerusage.csv') as data_file:
  for line in data_file:
    if is_header:
      is_header = False
      continue
    ms, ma = line.strip().split(',')
    ms = int(ms)
    ma = float(ma)

    if last_ms is None:
      dt = 1
    else:
      dt = ms - last_ms
    last_ms = ms

    if ms < 43059 or ms > 57451:
      logging_total[0] += dt
      logging_total[1] += ma * dt
    else:
      exposure_total[0] += dt
      exposure_total[1] += ma * dt

def Summarize(millis, milli_amp_seconds):
  print '%.2fms %.2fmA*s %.2fmAH' % (
      millis, milli_amp_seconds, milli_amp_seconds / (60.0 * 1000))

Summarize(*logging_total)
Summarize(*exposure_total)
