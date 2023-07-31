package com.google.android.apps.internal.games.memoryadvice;

import static com.google.android.apps.internal.games.memoryadvice.Utils.getOomScore;
import static com.google.android.apps.internal.games.memoryadvice.Utils.processMeminfo;
import static com.google.android.apps.internal.games.memoryadvice.Utils.processStatus;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Debug;
import android.util.Log;
import java.util.Map;
import org.json.JSONException;
import org.json.JSONObject;

/** A class to provide metrics of current memory usage to an application in JSON format. */
class MemoryMonitor {
  private static final String TAG = MemoryMonitor.class.getSimpleName();

  private static final long BYTES_IN_KILOBYTE = 1024;
  private static final long BYTES_IN_MEGABYTE = BYTES_IN_KILOBYTE * 1024;
  private final MapTester mapTester;
  protected final JSONObject baseline;
  private final ActivityManager activityManager;
  private int latestOnTrimLevel;

  /**
   * Create an Android memory metrics fetcher.
   *
   * @param context  The Android context to employ.
   * @param metrics The constant metrics to fetch - constant and variable.
   */
  MemoryMonitor(Context context, JSONObject metrics) {
    mapTester = new MapTester(context.getCacheDir());
    activityManager = (ActivityManager) context.getSystemService((Context.ACTIVITY_SERVICE));

    try {
      baseline = getMemoryMetrics(metrics.getJSONObject("variable"));
      JSONObject constant = metrics.optJSONObject("constant");
      if (constant != null) {
        baseline.put("constant", getMemoryMetrics(constant));
      }
    } catch (JSONException e) {
      throw new IllegalStateException(e);
    }
  }

  /**
   * Gets Android memory metrics.
   *
   * @param fields The fields to fetch in a JSON dictionary.
   * @return A JSONObject containing current memory metrics.
   */
  public JSONObject getMemoryMetrics(JSONObject fields) {
    JSONObject report = new JSONObject();
    try {
      if (fields.has("debug")) {
        JSONObject metricsOut = new JSONObject();
        Object debugFieldsValue = fields.get("debug");
        JSONObject debug =
            debugFieldsValue instanceof JSONObject ? (JSONObject) debugFieldsValue : null;
        boolean allFields = debugFieldsValue instanceof Boolean && (Boolean) debugFieldsValue;
        if (allFields || (debug != null && debug.optBoolean("nativeHeapAllocatedSize"))) {
          metricsOut.put("NativeHeapAllocatedSize", Debug.getNativeHeapAllocatedSize());
        }
        if (allFields || (debug != null && debug.optBoolean("NativeHeapFreeSize"))) {
          metricsOut.put("NativeHeapFreeSize", Debug.getNativeHeapFreeSize());
        }
        if (allFields || (debug != null && debug.optBoolean("NativeHeapSize"))) {
          metricsOut.put("NativeHeapSize", Debug.getNativeHeapSize());
        }
        if (allFields || (debug != null && debug.optBoolean("Pss"))) {
          metricsOut.put("Pss", Debug.getNativeHeapSize());
        }
        if (metricsOut.length() > 0) {
          report.put("debug", metricsOut);
        }
      }

      if (fields.has("MemoryInfo")) {
        Object memoryInfoValue = fields.get("MemoryInfo");
        JSONObject memoryInfoFields =
            memoryInfoValue instanceof JSONObject ? (JSONObject) memoryInfoValue : null;
        boolean allFields = memoryInfoValue instanceof Boolean && (Boolean) memoryInfoValue;
        if (allFields || (memoryInfoFields != null && memoryInfoFields.length() > 0)) {
          JSONObject metricsOut = new JSONObject();
          ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
          activityManager.getMemoryInfo(memoryInfo);
          if (allFields || memoryInfoFields.has("availMem")) {
            metricsOut.put("availMem", memoryInfo.availMem);
          }
          if ((allFields || memoryInfoFields.has("lowMemory")) && memoryInfo.lowMemory) {
            metricsOut.put("lowMemory", true);
          }
          if (allFields || memoryInfoFields.has("totalMem")) {
            metricsOut.put("totalMem", memoryInfo.totalMem);
          }
          if (allFields || memoryInfoFields.has("threshold")) {
            metricsOut.put("threshold", memoryInfo.threshold);
          }
          if (metricsOut.length() > 0) {
            report.put("MemoryInfo", metricsOut);
          }
        }
      }

      if (fields.has("ActivityManager")) {
        JSONObject activityManagerFields = fields.getJSONObject("ActivityManager");
        if (activityManagerFields.has("MemoryClass")) {
          report.put("MemoryClass", activityManager.getMemoryClass() * BYTES_IN_MEGABYTE);
        }
        if (activityManagerFields.has("LargeMemoryClass")) {
          report.put("LargeMemoryClass", activityManager.getLargeMemoryClass() * BYTES_IN_MEGABYTE);
        }
        if (activityManagerFields.has("LowRamDevice")) {
          report.put("LowRamDevice", activityManager.isLowRamDevice());
        }
      }

      if (mapTester.warning()) {
        report.put("mapTester", true);
        mapTester.reset();
      }

      if (latestOnTrimLevel > 0) {
        report.put("onTrim", latestOnTrimLevel);
        latestOnTrimLevel = 0;
      }

      int pid = android.os.Process.myPid();
      if (fields.has("proc")) {
        Object procFieldsValue = fields.get("proc");
        JSONObject procFields =
            procFieldsValue instanceof JSONObject ? (JSONObject) procFieldsValue : null;
        boolean allFields = procFieldsValue instanceof Boolean && (Boolean) procFieldsValue;
        JSONObject metricsOut = new JSONObject();
        if (allFields || procFields.optBoolean("oom_score")) {
          metricsOut.put("oom_score", getOomScore(pid));
        }
        if (metricsOut.length() > 0) {
          report.put("proc", metricsOut);
        }
      }

      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && fields.has("summary")) {
        Object summaryValue = fields.get("summary");
        JSONObject summary = summaryValue instanceof JSONObject ? (JSONObject) summaryValue : null;
        boolean allFields = summaryValue instanceof Boolean && (Boolean) summaryValue;
        if (allFields || (summary != null && summary.length() > 0)) {
          Debug.MemoryInfo[] debugMemoryInfos =
              activityManager.getProcessMemoryInfo(new int[] {pid});
          JSONObject metricsOut = new JSONObject();
          for (Debug.MemoryInfo debugMemoryInfo : debugMemoryInfos) {
            for (Map.Entry<String, String> entry : debugMemoryInfo.getMemoryStats().entrySet()) {
              String key = entry.getKey();
              if (allFields || summary.has(key)) {
                long value = Long.parseLong(entry.getValue()) * BYTES_IN_KILOBYTE;
                metricsOut.put(key, metricsOut.optLong(key) + value);
              }
            }
          }
          if (metricsOut.length() > 0) {
            report.put("summary", metricsOut);
          }
        }
      }

      if (fields.has("meminfo")) {
        Object meminfoFieldsValue = fields.get("meminfo");
        JSONObject meminfoFields =
            meminfoFieldsValue instanceof JSONObject ? (JSONObject) meminfoFieldsValue : null;
        boolean allFields = meminfoFieldsValue instanceof Boolean && (Boolean) meminfoFieldsValue;
        if (allFields || (meminfoFields != null && meminfoFields.length() > 0)) {
          JSONObject metricsOut = new JSONObject();
          Map<String, Long> memInfo = processMeminfo();
          for (Map.Entry<String, Long> pair : memInfo.entrySet()) {
            String key = pair.getKey();
            if (allFields || meminfoFields.optBoolean(key)) {
              metricsOut.put(key, pair.getValue());
            }
          }
          if (metricsOut.length() > 0) {
            report.put("meminfo", metricsOut);
          }
        }
      }

      if (fields.has("status")) {
        Object statusValue = fields.get("status");
        JSONObject status = statusValue instanceof JSONObject ? (JSONObject) statusValue : null;
        boolean allFields = statusValue instanceof Boolean && (Boolean) statusValue;
        if (allFields || (status != null && status.length() > 0)) {
          JSONObject metricsOut = new JSONObject();
          for (Map.Entry<String, Long> pair : processStatus(pid).entrySet()) {
            String key = pair.getKey();
            if (allFields || status.optBoolean(key)) {
              metricsOut.put(key, pair.getValue());
            }
          }
          if (metricsOut.length() > 0) {
            report.put("status", metricsOut);
          }
        }
      }

      JSONObject meta = new JSONObject();
      meta.put("time", System.currentTimeMillis());
      report.put("meta", meta);
    } catch (JSONException ex) {
      Log.w(TAG, "Problem getting memory metrics", ex);
    }
    return report;
  }

  public void setOnTrim(int level) {
    if (level > latestOnTrimLevel) {
      latestOnTrimLevel = level;
    }
  }
}
