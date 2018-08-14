#!/usr/bin/env python3
"""Copies all files recursively under `src/` as flat files under `dst/`.

Creates `dst/` if necessary. Renames files sequentially.

When a Nikon dSLR is set up for this time lapse controller, it is always set to
on but has its external power cycled. As a result it starts a new folder on the
SD card each time it powers on and takes a photo; but it also restarts the
image sequence number within each folder, resulting in:
  FOLDER01/DSC_0001.NEF
  FOLDER02/DSC_0001.NEF
etc. This script is to unpack and renumber those photos:
  OUT/00001.NEF
  OUT/00002.NEF
"""
import argparse
import logging
import os
import shutil
import sys


def _Copy(src_filename, dst_filename):
  logging.info('cp %r %r', src_filename, dst_filename)
  if os.path.exists(dst_filename):
    logging.error(
        'Destination file %r already exists, skipping %r.',
        dst_filename, src_filename)
    return False
  shutil.copy(src_filename, dst_filename)
  return True


def FlattenDirs(src, dst):
  if not os.path.isdir(dst):
    logging.info('Creating %r.', dst)
    os.makedirs(dst)

  n = 0
  not_copied = []
  for dirpath, dirnames, filenames in os.walk(src):
    for src_local_filename in filenames:
      _, ext = os.path.splitext(src_filename)
      src_filename = os.path.join(dirpath, src_local_filename)
      dst_filename = os.path.join(dst, '%05%s' % (n, ext))
      if not _Copy(src_filename, dst_filename):
        not_copied.append(src_filename)
      n += 1

  logging.info('Copied %d files.', n)
  if not_copied:
    logging.warning(
        'Did not copy %d files:\n\t%s',
        len(not_copied), '\n\t'.join(not_copied))


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(levelname)s %(name)s] %(message)s',
      level=logging.INFO)

  summary_line, _, main_doc = __doc__.partition('\n\n')
  parser = argparse.ArgumentParser(
      description=summary_line,
      epilog=main_doc,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      'src',
      help='Search recursively in this source directory for files to copy.')
  parser.add_argument(
      'dst',
      help='Copy (and rename) files to this directory, which may be created.')
  args = parser.parse_args()

  FlattenDirs(args.src, args.dst)
