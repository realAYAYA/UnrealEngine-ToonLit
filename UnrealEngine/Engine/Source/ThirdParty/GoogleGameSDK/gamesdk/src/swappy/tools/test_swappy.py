# Lint as: python3
"""Check whether Swappy is working properly.

This script takes paths to a logcat and a systrace and applies checks on them to
verify whether Swappy is working properly or not. A zero return code means all
looks fine, while other codes indicate different problems.

To create the logcat file:
  adb logcat -d > $filename$
To create the systrace file:
  python systrace/systrace.py -e $device ID$ -o $output file$ -t $length in seconds$ gfx
"""

import sys

import argparse

import lib_test_swappy

RET_CODE_SWAPPY_NOT_ON = 10
RET_CODE_SWAPPY_NOT_WORKING = 11
RET_CODE_SWAPPY_NO_FRAME_STATS = 12

def main():
  """
  Main function
  """
  parser = argparse.ArgumentParser(description='Check Swappy logcat and systrace output')
  parser.add_argument('--logcat', type=str, help='logcat file', required=True)
  parser.add_argument('--systrace', type=str, help='systrace file')
  parser.add_argument('--latency_thr_ms', type=float, default=25.0,
                      help='Latency threshold in milliseconds')
  parser.add_argument('--no_frame_stats', action='store_true', help="Don't check frame statistics")

  args = parser.parse_args()

  print('logcat:', args.logcat)
  print('systrace:', args.systrace)

  with open(args.logcat, 'rb') as logcat_file:
    logcat = logcat_file.readlines()

  swappy_backend = lib_test_swappy.test_swappy_initialized(logcat)
  if not swappy_backend:
    sys.exit(RET_CODE_SWAPPY_NOT_ON)
  print("Swappy choreographer backend = %s" % swappy_backend)

  if not args.no_frame_stats:
    if not lib_test_swappy.test_swappy_frame_stats(logcat):
      sys.exit(RET_CODE_SWAPPY_NO_FRAME_STATS)

  if args.systrace:
    if not lib_test_swappy.test_swappy_working(logcat, args.systrace, args.latency_thr_ms):
      sys.exit(RET_CODE_SWAPPY_NOT_WORKING)

if __name__ == '__main__':
  main()
