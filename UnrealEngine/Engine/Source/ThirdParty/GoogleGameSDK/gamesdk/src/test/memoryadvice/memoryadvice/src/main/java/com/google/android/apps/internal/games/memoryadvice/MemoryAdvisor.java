package com.google.android.apps.internal.games.memoryadvice;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.IOException;
import java.util.Iterator;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import static com.google.android.apps.internal.games.memoryadvice.Utils.readStream;

/**
 * Wrapper class for methods related to memory advice.
 */
public class MemoryAdvisor extends MemoryMonitor {
  private static final String TAG = MemoryMonitor.class.getSimpleName();
  private final JSONObject deviceProfile;
  private final JSONObject params;

  /**
   * Create an Android memory advice fetcher.
   *
   * @param context The Android context to employ.
   */
  public MemoryAdvisor(Context context) {
    this(context, getDefaultParams(context.getAssets()));
  }

  private static JSONObject getDefaultParams(AssetManager assets) {
    JSONObject params;
    try {
      params = new JSONObject(readStream(assets.open("memoryadvice/default.json")));
    } catch (JSONException | IOException ex) {
      Log.e(TAG, "Problem getting default params", ex);
      params = new JSONObject();
    }
    return params;
  }

  /**
   * Create an Android memory advice fetcher.
   *
   * @param context The Android context to employ.
   * @param params The active configuration.
   */
  public MemoryAdvisor(Context context, JSONObject params) {
    super(context, params.optJSONObject("metrics"));
    this.params = params;
    deviceProfile = DeviceProfile.getDeviceProfile(context.getAssets(), params, baseline);
  }

  /**
   * Returns 'true' if there are any low memory warnings in the advice object.
   * @deprecated since 0.7. Use getMemoryState() instead.
   * @param advice The advice object returned by getAdvice().
   * @return if there are any low memory warnings in the advice object.
   */
  @Deprecated
  public static boolean anyWarnings(JSONObject advice) {
    JSONArray warnings = advice.optJSONArray("warnings");
    return warnings != null && warnings.length() > 0;
  }

  /**
   * Returns an estimate for the amount of memory that can safely be allocated,
   * in bytes.
   * @param advice The advice object returned by getAdvice().
   * @return an estimate for the amount of memory that can safely be allocated,
   * in bytes. 0 if no estimate is available.
   */
  public static long availabilityEstimate(JSONObject advice) {
    if (!advice.has("predictions")) {
      return 0;
    }
    try {
      long smallestEstimate = Long.MAX_VALUE;
      JSONObject predictions = advice.getJSONObject("predictions");
      Iterator<String> it = predictions.keys();
      if (!it.hasNext()) {
        return 0;
      }
      do {
        String key = it.next();
        smallestEstimate = Math.min(smallestEstimate, predictions.getLong(key));
      } while (it.hasNext());
      return smallestEstimate;
    } catch (JSONException ex) {
      Log.w(TAG, "Problem getting memory estimate", ex);
    }
    return 0;
  }

  /**
   * Return 'true' if there are any 'red' (critical) warnings in the advice object.
   * @deprecated since 0.7. Use getMemoryState() instead.
   * @param advice The advice object returned by getAdvice().
   * @return if there are any 'red' (critical) warnings in the advice object.
   */
  @Deprecated
  public static boolean anyRedWarnings(JSONObject advice) {
    JSONArray warnings = advice.optJSONArray("warnings");
    if (warnings == null) {
      return false;
    }

    for (int idx = 0; idx != warnings.length(); idx++) {
      JSONObject warning = warnings.optJSONObject(idx);
      if (warning != null && "red".equals(warning.optString("level"))) {
        return true;
      }
    }
    return false;
  }

  /**
   * Get the memory state from an advice object returned by the Memory Advisor.
   * @param advice The object to analyze for the memory state.
   * @return The current memory state.
   */
  public static MemoryState getMemoryState(JSONObject advice) {
    if (anyRedWarnings(advice)) {
      return MemoryState.CRITICAL;
    }
    if (anyWarnings(advice)) {
      return MemoryState.APPROACHING_LIMIT;
    }
    return MemoryState.OK;
  }

  /**
   * Find a Long in a JSON object, even when it is nested in sub-dictionaries in the object.
   * @param object The object to search.
   * @param key The key of the Long to find.
   * @return The value of he Long.
   */
  private static Long getValue(JSONObject object, String key) {
    try {
      if (object.has(key)) {
        return object.getLong(key);
      }
      Iterator<String> it = object.keys();
      while (it.hasNext()) {
        Object value = object.get(it.next());
        if (value instanceof JSONObject) {
          Long value1 = getValue((JSONObject) value, key);
          if (value1 != null) {
            return value1;
          }
        }
      }
    } catch (JSONException ex) {
      Log.w(TAG, "Problem fetching value", ex);
    }
    return null;
  }

  /**
   * The value the advisor returns when asked for memory pressure on the device through the
   * getSignal method. GREEN indicates it is safe to allocate further, YELLOW indicates further
   * allocation shouldn't happen, and RED indicates high memory pressure.
   */
  public JSONObject getAdvice() {
    long time = System.currentTimeMillis();
    JSONObject results = new JSONObject();

    try {
      JSONObject metricsParams = params.getJSONObject("metrics");
      JSONObject metrics = getMemoryMetrics(metricsParams.getJSONObject("variable"));
      results.put("metrics", metrics);
      JSONObject limits = deviceProfile.getJSONObject("limits");
      JSONObject deviceLimit = limits.getJSONObject("limit");
      JSONObject deviceBaseline = limits.getJSONObject("baseline");

      if (params.has("heuristics")) {
        JSONArray warnings = new JSONArray();
        JSONObject heuristics = params.getJSONObject("heuristics");
        if (heuristics.has("try")) {
          if (!TryAllocTester.tryAlloc((int) Utils.getMemoryQuantity(heuristics.get("try")))) {
            JSONObject warning = new JSONObject();
            warning.put("try", heuristics.get("try"));
            warning.put("level", "red");
            warnings.put(warning);
          }
        }

        if (heuristics.has("lowMemory")) {
          if (metrics.optBoolean("lowMemory")) {
            JSONObject warning = new JSONObject();
            warning.put("lowMemory", heuristics.get("lowMemory"));
            warning.put("level", "red");
            warnings.put(warning);
          }
        }

        if (heuristics.has("mapTester")) {
          if (metrics.optBoolean("mapTester")) {
            JSONObject warning = new JSONObject();
            warning.put("mapTester", heuristics.get("mapTester"));
            warning.put("level", "red");
            warnings.put(warning);
          }
        }

        if (heuristics.has("onTrim")) {
          if (metrics.optInt("onTrim") > 0) {
            JSONObject warning = new JSONObject();
            warning.put("onTrim", heuristics.get("onTrim"));
            warning.put("level", "red");
            warnings.put(warning);
          }
        }

        // Handler for device-based metrics.
        Iterator<String> it = heuristics.keys();
        while (it.hasNext()) {
          String key = it.next();
          JSONObject heuristic;
          try {
            heuristic = heuristics.getJSONObject(key);
          } catch (JSONException e) {
            break;
          }

          Long metricValue = getValue(metrics, key);
          if (metricValue == null) {
            continue;
          }

          Long deviceLimitValue = getValue(deviceLimit, key);
          if (deviceLimitValue == null) {
            continue;
          }

          Long deviceBaselineValue = getValue(deviceBaseline, key);
          if (deviceBaselineValue == null) {
            continue;
          }

          Long baselineValue = getValue(baseline, key);
          if (baselineValue == null) {
            continue;
          }

          boolean increasing = deviceLimitValue > deviceBaselineValue;

          // Fires warnings as metrics approach absolute values.
          // Example: "Active": {"fixed": {"red": "300M", "yellow": "400M"}}
          if (heuristic.has("fixed")) {
            JSONObject fixed = heuristic.getJSONObject("fixed");
            long red = Utils.getMemoryQuantity(fixed.get("red"));
            long yellow = Utils.getMemoryQuantity(fixed.get("yellow"));
            String level = null;
            if (increasing ? metricValue > red : metricValue < red) {
              level = "red";
            } else if (increasing ? metricValue > yellow : metricValue < yellow) {
              level = "yellow";
            }
            if (level != null) {
              JSONObject warning = new JSONObject();
              JSONObject trigger = new JSONObject();
              trigger.put("fixed", fixed);
              warning.put(key, trigger);
              warning.put("level", level);
              warnings.put(warning);
            }
          }

          // Fires warnings as metrics approach ratios of the device baseline.
          // Example: "availMem": {"baselineRatio": {"red": 0.30, "yellow": 0.40}}
          if (heuristic.has("baselineRatio")) {
            JSONObject baselineRatio = heuristic.getJSONObject("baselineRatio");

            String level = null;
            if (increasing ? metricValue > baselineValue * baselineRatio.getDouble("red")
                           : metricValue < baselineValue * baselineRatio.getDouble("red")) {
              level = "red";
            } else if (increasing
                    ? metricValue > baselineValue * baselineRatio.getDouble("yellow")
                    : metricValue < baselineValue * baselineRatio.getDouble("yellow")) {
              level = "yellow";
            }
            if (level != null) {
              JSONObject warning = new JSONObject();
              JSONObject trigger = new JSONObject();
              trigger.put("baselineRatio", baselineRatio);
              warning.put(key, trigger);
              warning.put("level", level);
              warnings.put(warning);
            }
          }

          // Fires warnings as baseline-relative metrics approach ratios of the device's baseline-
          // relative limit.
          // Example: "oom_score": {"deltaLimit": {"red": 0.85, "yellow": 0.75}}
          if (heuristic.has("deltaLimit")) {
            JSONObject deltaLimit = heuristic.getJSONObject("deltaLimit");
            long limitValue = deviceLimitValue - deviceBaselineValue;
            long relativeValue = metricValue - baselineValue;
            String level = null;
            if (increasing ? relativeValue > limitValue * deltaLimit.getDouble("red")
                           : relativeValue < limitValue * deltaLimit.getDouble("red")) {
              level = "red";
            } else if (increasing ? relativeValue > limitValue * deltaLimit.getDouble("yellow")
                                  : relativeValue < limitValue * deltaLimit.getDouble("yellow")) {
              level = "yellow";
            }
            if (level != null) {
              JSONObject warning = new JSONObject();
              JSONObject trigger = new JSONObject();
              trigger.put("deltaLimit", deltaLimit);
              warning.put(key, trigger);
              warning.put("level", level);
              warnings.put(warning);
            }
          }

          // Fires warnings as metrics approach ratios of the device's limit.
          // Example: "VmRSS": {"deltaLimit": {"red": 0.90, "yellow": 0.75}}
          if (heuristic.has("limit")) {
            JSONObject limit = heuristic.getJSONObject("limit");
            String level = null;
            if (increasing ? metricValue > deviceLimitValue * limit.getDouble("red")
                           : metricValue * limit.getDouble("red") < deviceLimitValue) {
              level = "red";
            } else if (increasing ? metricValue > deviceLimitValue * limit.getDouble("yellow")
                                  : metricValue * limit.getDouble("yellow") < deviceLimitValue) {
              level = "yellow";
            }
            if (level != null) {
              JSONObject warning = new JSONObject();
              JSONObject trigger = new JSONObject();
              trigger.put("limit", limit);
              warning.put(key, trigger);
              warning.put("level", level);
              warnings.put(warning);
            }
          }
        }

        if (warnings.length() > 0) {
          results.put("warnings", warnings);
        }
      }

      if (deviceLimit.has("stressed")) {
        JSONObject stressed = deviceLimit.getJSONObject("stressed");
        if (stressed.has("applicationAllocated")) {
          long applicationAllocated = stressed.getLong("applicationAllocated");
          JSONObject predictions = new JSONObject();
          JSONObject predictionsParams = params.getJSONObject("predictions");

          Iterator<String> it = predictionsParams.keys();

          while (it.hasNext()) {
            String key = it.next();

            Long metricValue = getValue(metrics, key);
            if (metricValue == null) {
              continue;
            }

            Long deviceLimitValue = getValue(deviceLimit, key);
            if (deviceLimitValue == null) {
              continue;
            }

            Long deviceBaselineValue = getValue(deviceBaseline, key);
            if (deviceBaselineValue == null) {
              continue;
            }

            Long baselineValue = getValue(baseline, key);
            if (baselineValue == null) {
              continue;
            }

            long delta = metricValue - baselineValue;
            long deviceDelta = deviceLimitValue - deviceBaselineValue;
            if (deviceDelta == 0) {
              continue;
            }

            float percentageEstimate = (float) delta / deviceDelta;
            predictions.put(key, (long) (applicationAllocated * (1.0f - percentageEstimate)));
          }

          results.put("predictions", predictions);
          JSONObject meta = new JSONObject();
          meta.put("duration", System.currentTimeMillis() - time);
          results.put("meta", meta);
        }
      }
    } catch (JSONException ex) {
      Log.w(TAG, "Problem performing memory analysis", ex);
    }
    return results;
  }

  /**
   * Fetch information about the device.
   * @param context The Android context.
   * @return Information about the device, in a JSONObject.
   */
  public JSONObject getDeviceInfo(Context context) {
    JSONObject deviceInfo = new JSONObject();
    try {
      deviceInfo.put("build", BuildInfo.getBuild(context));
      deviceInfo.put("baseline", baseline);
      deviceInfo.put("deviceProfile", deviceProfile);
      deviceInfo.put("params", params);
    } catch (JSONException ex) {
      Log.w(TAG, "Problem getting device info", ex);
    }
    return deviceInfo;
  }

  /**
   * Advice passed from the memory advisor to the application about the state of memory.
   */
  public enum MemoryState {
    /**
     * The memory state cannot be determined.
     */
    UNKNOWN,

    /**
     * The application can safely allocate significant memory.
     */
    OK,

    /**
     * The application should not allocate significant memory.
     */
    APPROACHING_LIMIT,

    /**
     * The application should free memory as soon as possible, until the memory state changes.
     */
    CRITICAL
  }
}
