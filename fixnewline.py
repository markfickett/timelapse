name = '20150505noref.csv'
fixed_name = '20150505noref-fixed.csv'

with open(name) as file:
  with open(fixed_name, 'w') as fixed_file:
    i = 0
    prev_line = ''
    for line in file:
      if prev_line:
        prev_line = '%s,%s' % (prev_line.strip(), line)
      else:
        prev_line = line
      i += 1
      #print '%d %r %r' % (i, line, prev_line)
      if i % 2 == 0:
        fixed_file.write(prev_line)
        prev_line = ''
