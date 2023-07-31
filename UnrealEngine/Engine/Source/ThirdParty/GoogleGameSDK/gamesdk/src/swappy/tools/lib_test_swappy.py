# Lint as: python3
"""Check whether Swappy is working properly.

This script takes a logcat (list of lines - e.g. from readlines()) and the path
to a systrace and applies checks on them to verify whether Swappy is working
properly or not. A zero return code means all looks fine, while other codes
indicate different problems.
"""

import decimal
import re
from typing import List

# Beautiful Soup parsing library
import bs4

def test_swappy_initialized(logcat: List[bytes]) -> str:
  """Verify whether Swappy initialized successfully

  Takes logcat data argument and searches for FrameStatistics blocks like this
  example:

  Args:
    logcat: Logcat data.

  Returns:
    The Swappy backend name or an empty string if Swappy is not initialized.
  """
  choreographer_re = re.compile(rb'Using (\S+) Choreographer')
  choreographer_backend = ''
  for line in logcat:
    match = choreographer_re.search(line)
    if match:
      choreographer_backend = match.group(1)

  return choreographer_backend

# pylint: disable=line-too-long
def test_swappy_frame_stats(logcat: List[bytes]) -> bool:
  """Verify whether Swappy is producing frame statistics.

  Takes logcat data argument and searches for FrameStatistics blocks like this
  example:

  12-02 00:24:47.630: I/FrameStatistics(15134): == Frame statistics ==
  12-02 00:24:47.630: I/FrameStatistics(15134): total frames: 123
  12-02 00:24:47.630: I/FrameStatistics(15134): Buckets:                      [0] [1] [2] [3] [4] [5]
  12-02 00:24:47.630: I/FrameStatistics(15134): idle frames:                   116   7   0   0   0   0
  12-02 00:24:47.630: I/FrameStatistics(15134): late frames:                   121   1   1   0   0   0
  12-02 00:24:47.630: I/FrameStatistics(15134): offset from previous frame:    122   0   0   0   0   0
  12-02 00:24:47.630: I/FrameStatistics(15134): frame latency:                 0   121   1   1   0   0

  A basic consistency check is applied: successive "total frames" values should
  never decrease. If not even a single block is found, Swappy is deemed to be
  off.

  Args:
    logcat: Logcat data.

  Returns:
    True if it looks like Swappy is on
  """
  # pylint: enable=line-too-long
  frame_stats_re = re.compile(rb'I/FrameStatistics\(\d+\): total frames: (\d+)')
  total_frames = -1
  for line in logcat:
    match = frame_stats_re.search(line)
    if match:
      new_total_frames = int(match.group(1))
      if new_total_frames < total_frames:
        return False
      total_frames = new_total_frames

  print(dict(total_frames=total_frames))
  return True if total_frames > -1 else False


def test_swappy_working(
    logcat: List[bytes], systrace_path: str, latency_thr_ms: float) -> bool:
  """Verify whether Swappy seems to be working correctly.

  For more details on this algorithm please see: http://go/swappy-systrace.

  Args:
    logcat: Logcat data.
    systrace_path: Path to the systrace file.
    latency_thr_ms: Maximum average queue latency expected if Swappy is working.

  Returns:
    True if it looks like Swappy is working correctly.
  """
  # Finding out surfaceflinger's PID from the logcat is way easier than from the
  # systrace
  sf_pid = get_surfaceflinger_pid(logcat)
  if sf_pid is None:
    print("Couldn't find surface flinger pid in logcat")

  trace_data = get_trace_data(systrace_path)
  soup = bs4.BeautifulSoup(trace_data, features='html.parser')
  script_tags = soup.find_all('script', attrs={'class': 'trace-data'})

  # The number of trace-data script tags apparently depends on device model.
  # If there's more than one, we look at the longest one because probably
  # (hopefully) that's where the actual trace is.
  trace_tag = max([(len(tag.string), tag) for tag in script_tags])[1]
  data = trace_tag.string.split('\n')
  data = [line.strip() for line in data if line.strip() and line[0] != '#']

  # We are interested in COUNTER events for the line named:
  # 'SurfaceView - <PACKAGE>/<ACTIVITY>'
  # Examples seen so far illustrate the expected possible formats:
  # pylint: disable=line-too-long
  #           <...>-3194  (-----) [001] ...1 33425.567500: tracing_mark_write: C|3194|SurfaceView - com.gameloft.android.ANMP.GloftA8HM/com.gameloft.android.ANMP.GloftA8HM.MainActivity@85d2511@0#0|0
  #  surfaceflinger-3194  ( 3194) [001] ...1 33662.535536: tracing_mark_write: C|3194|SurfaceView - com.gameloft.android.ANMP.GloftA8HM/com.gameloft.android.ANMP.GloftA8HM.MainActivity@73c2e5f@0#0|0
  #    Binder:486_2-533   (  486) [000] ...1 69410.718619: tracing_mark_write: C|486|SurfaceView - com.prefabulated.swappy/com.prefabulated.bouncyball.OrbitActivity#0|2
  # pylint: enable=line-too-long
  if sf_pid is not None:
    event_re_str = (
        r' (?P<time>\d+\.\d{6}): tracing_mark_write: C\|'
        + str(sf_pid)
        + r'\|SurfaceView - .*\|(?P<value>\d+)')
  else:
    event_re_str = (
        r' (?P<time>\d+\.\d{6}): tracing_mark_write: C\|\d+\|SurfaceView - .*\|(?P<value>\d+)')
  event_re = re.compile(event_re_str)

  current_time_s = None
  current_depth = None
  enqueued_frames = 0
  total_queuing_time_s = decimal.Decimal(0)
  for line in data:
    match = event_re.search(line)
    if match:
      new_time_s = decimal.Decimal(match.group('time'))
      new_depth = int(match.group('value'))
      if current_depth is not None:
        if new_depth > current_depth:
          enqueued_frames += new_depth - current_depth
        if current_depth > 0:
          total_queuing_time_s += current_depth * (new_time_s - current_time_s)
      current_time_s = new_time_s
      current_depth = new_depth

  total_queuing_time_ms = total_queuing_time_s * 1000
  avg_queue_latency_ms = total_queuing_time_ms / enqueued_frames
  print(dict(
      enqueued_frames=enqueued_frames,
      total_queuing_time_ms=float(total_queuing_time_ms),
      latency_thr_ms=latency_thr_ms,
      avg_queue_latency_ms=float(avg_queue_latency_ms)))
  return avg_queue_latency_ms < latency_thr_ms

def get_surfaceflinger_pid(logcat: List[bytes]) -> int:
  """Obtain the surfaceflinger PID from the logcat.

  All instances of the PID in the file must be the same, or an AssertionError
  will be raised.

  Args:
    logcat: Logcat data.

  Returns:
    surfaceflinger PID number
  """
  # Look for e.g.
  #  12-03 12:44:37.667: W/SurfaceFlinger(1234):'
  sf_pid_re = re.compile(
      rb'\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d: [WIDE]\/SurfaceFlinger\((\d+)\): ')
  # Sometimes adb logcat gives a different format, like this:
  #  12-03 12:44:37.667   602  1641 W SurfaceFlinger'
  sf_pid_2_re = re.compile(rb'\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d\s+(\d+)\s+(\d+)\s+[WIDE]\s+SurfaceFlinger')
  sf_pid = None
  for line in logcat:
    match = sf_pid_re.search(line)
    if not match:
      match = sf_pid_2_re.search(line)
    if match:
      this_pid = int(match.group(1))
      if sf_pid is None:
        sf_pid = this_pid
      else:
        assert sf_pid == this_pid
  return sf_pid


def get_trace_data(systrace_path: str) -> bytes:
  """Open file and get just the trace data (ignoring HTML data and etc.)."""
  with open(systrace_path, 'rb') as f:
    file_data = f.read()
  start_marker = b'<!-- BEGIN TRACE -->'
  end_marker = b'<!-- END TRACE -->'
  start_pos = file_data.index(start_marker) + len(start_marker)
  end_pos = file_data.index(end_marker)
  return file_data[start_pos:end_pos]
