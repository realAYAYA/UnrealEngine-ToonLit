package com.google.android.apps.internal.games.memoryadvice;

import android.app.ActivityManager;
import android.os.Debug;
import android.util.Log;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** A helper class with static methods to help with Heuristics and file IO */
public class Utils {
  private static final String TAG = Utils.class.getSimpleName();
  private static final Pattern MEMINFO_REGEX = Pattern.compile("([^:]+)[^\\d]*(\\d+).*\n");
  private static final Pattern PROC_REGEX = Pattern.compile("([a-zA-Z]+)[^\\d]*(\\d+) kB.*\n");

  /**
   * Loads all the text from an input string and returns the result as a string.
   *
   * @param inputStream The stream to read.
   * @return All of the text from the stream.
   * @throws IOException Thrown if a read error occurs.
   */
  public static String readStream(InputStream inputStream) throws IOException {
    try (InputStreamReader inputStreamReader = new InputStreamReader(inputStream);
         BufferedReader reader = new BufferedReader(inputStreamReader)) {
      String newline = System.lineSeparator();
      StringBuilder output = new StringBuilder();
      String line;
      while ((line = reader.readLine()) != null) {
        if (output.length() > 0) {
          output.append(newline);
        }
        output.append(line);
      }
      return output.toString();
    }
  }

  /**
   * Loads all text from the specified file and returns the result as a string.
   *
   * @param filename The name of the file to read.
   * @return All of the text from the file.
   * @throws IOException Thrown if a read error occurs.
   */
  public static String readFile(String filename) throws IOException {
    return readStream(new FileInputStream(filename));
  }

  /**
   * Returns the OOM score associated with a process.
   *
   * @return The OOM score, or -1 if the score cannot be obtained.
   * @param pid The process ID (pid).
   */
  static int getOomScore(int pid) {
    try {
      return Integer.parseInt(readFile(("/proc/" + pid) + "/oom_score"));
    } catch (IOException | NumberFormatException e) {
      return -1;
    }
  }

  /**
   * Returns a dictionary of values extracted from the /proc/meminfo file.
   *
   * @return A dictionary of values, in bytes.
   */
  static Map<String, Long> processMeminfo() {
    Map<String, Long> output = new HashMap<>();

    String filename = "/proc/meminfo";
    try {
      String meminfoText = readFile(filename);
      Matcher matcher = MEMINFO_REGEX.matcher(meminfoText);
      while (matcher.find()) {
        output.put(
            matcher.group(1), Long.parseLong(Objects.requireNonNull(matcher.group(2))) * 1024);
      }
    } catch (IOException e) {
      Log.w(TAG, "Failed to read " + filename);
    }
    return output;
  }

  /**
   * Return a dictionary of values extracted from a processes' /proc/../status
   * files.
   *
   * @param pid The process ID (pid).
   * @return A dictionary of values, in bytes.
   */
  static Map<String, Long> processStatus(int pid) {
    Map<String, Long> output = new HashMap<>();
    String filename = "/proc/" + pid + "/status";
    try {
      String meminfoText = readFile(filename);
      Matcher matcher = PROC_REGEX.matcher(meminfoText);
      while (matcher.find()) {
        String key = matcher.group(1);
        long value = Long.parseLong(Objects.requireNonNull(matcher.group(2)));
        output.put(key, value * 1024);
      }
    } catch (IOException e) {
      Log.w(TAG, "Failed to read " + filename);
    }
    return output;
  }

  /**
   * Converts a memory quantity value in an object to a number of bytes. If the value is a number,
   * it is interpreted as the number of bytes. If the value is a string, it is converted according
   * to the specified unit. e.g. "36K", "52.5 m", "9.1G". No unit is interpreted as bytes.
   *
   * @param object The object to extract from.
   * @return The equivalent number of bytes.
   */
  public static long getMemoryQuantity(Object object) {
    if (object instanceof Number) {
      return ((Number) object).longValue();
    }

    if (object instanceof String) {
      String str = ((String) object).toUpperCase();
      int unitPosition = str.indexOf('K');
      long unitMultiplier = 1024;
      if (unitPosition == -1) {
        unitPosition = str.indexOf('M');
        unitMultiplier *= 1024;
        if (unitPosition == -1) {
          unitPosition = str.indexOf('G');
          unitMultiplier *= 1024;
          if (unitPosition == -1) {
            unitMultiplier = 1;
          }
        }
      }
      float value = Float.parseFloat(str.substring(0, unitPosition));
      return (long) (value * unitMultiplier);
    }
    throw new IllegalArgumentException("Input to getMemoryQuantity neither string or number.");
  }
}
