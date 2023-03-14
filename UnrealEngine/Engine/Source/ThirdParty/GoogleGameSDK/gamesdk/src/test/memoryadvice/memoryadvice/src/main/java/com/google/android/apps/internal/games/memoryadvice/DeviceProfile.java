package com.google.android.apps.internal.games.memoryadvice;

import static com.google.android.apps.internal.games.memoryadvice.Utils.readStream;

import android.content.res.AssetManager;
import android.os.Build;
import android.util.Log;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.SortedSet;
import java.util.TreeSet;
import org.json.JSONException;
import org.json.JSONObject;

class DeviceProfile {
  /**
   * Return the first index where two strings differ.
   * @param a The first string to compare.
   * @param b The second string to compare.
   * @return The first index where the two strings have a different character, or either terminate.
   */
  private static int mismatchIndex(CharSequence a, CharSequence b) {
    int index = 0;
    while (true) {
      if (index >= a.length() || index >= b.length()) {
        return index;
      }
      if (a.charAt(index) != b.charAt(index)) {
        return index;
      }
      index++;
    }
  }

  /**
   * Selects the device profile from all available.
   * @param assets The assert manager used to load files.
   * @param params Configuration parameters (can affect selection method).
   * @param baseline This device's baseline metrics (can be used to aid selection).
   * @return The selected device profile, plus metadata.
   */
  static JSONObject getDeviceProfile(AssetManager assets, JSONObject params, JSONObject baseline) {
    JSONObject profile = new JSONObject();
    try {
      JSONObject lookup = new JSONObject(readStream(assets.open("memoryadvice/lookup.json")));
      String matchStrategy = params.optString("matchStrategy", "fingerprint");
      String best;
      if ("fingerprint".equals(matchStrategy)) {
        best = matchByFingerprint(lookup);
      } else if ("baseline".equals(matchStrategy)) {
        best = matchByBaseline(lookup, baseline);
      } else {
        throw new IllegalStateException("Unknown match strategy " + matchStrategy);
      }
      profile.put("limits", lookup.getJSONObject(best));
      profile.put("matched", best);
      profile.put("fingerprint", Build.FINGERPRINT);
    } catch (JSONException | IOException e) {
      Log.w("Profile problem.", e);
    }
    return profile;
  }

  /**
   * This method finds the device with the most similar fingerprint string.
   * @param lookup The lookup table.
   * @return The selected device.
   */
  private static String matchByFingerprint(JSONObject lookup) {
    int bestScore = -1;
    String best = null;
    Iterator<String> it = lookup.keys();
    while (it.hasNext()) {
      String key = it.next();
      int score = mismatchIndex(Build.FINGERPRINT, key);
      if (score > bestScore) {
        bestScore = score;
        best = key;
      }
    }
    return best;
  }

  /**
   * this method finds the device with the most similar baseline metrics.
   * @param lookup The lookup table.
   * @param baseline The current device metrics baseline.
   * @return The selected device.
   */
  private static String matchByBaseline(JSONObject lookup, JSONObject baseline)
      throws JSONException {
    Map<String, SortedSet<Long>> baselineValuesTable = buildBaselineValuesTable(lookup);

    float bestScore = Float.MAX_VALUE;
    String best = null;

    Iterator<String> it = lookup.keys();
    while (it.hasNext()) {
      String key = it.next();
      JSONObject limits = lookup.getJSONObject(key);
      JSONObject prospectBaseline = limits.getJSONObject("baseline");

      float totalScore = 0;
      int totalUnion = 0;
      Iterator<String> it2 = prospectBaseline.keys();
      while (it2.hasNext()) {
        String groupName = it2.next();
        if (!baseline.has(groupName)) {
          continue;
        }
        JSONObject prospectBaselineGroup = prospectBaseline.getJSONObject(groupName);
        JSONObject baselineGroup = baseline.getJSONObject(groupName);
        Iterator<String> it3 = prospectBaselineGroup.keys();
        while (it3.hasNext()) {
          String metric = it3.next();
          if (!baselineGroup.has(metric)) {
            continue;
          }
          totalUnion++;
          SortedSet<Long> values = baselineValuesTable.get(metric);
          int prospectPosition = getPositionInList(values, prospectBaselineGroup.getLong(metric));
          int ownPosition = getPositionInList(values, baselineGroup.getLong(metric));
          float score = (float) Math.abs(prospectPosition - ownPosition) / values.size();
          totalScore += score;
        }
      }

      if (totalUnion > 0) {
        totalScore /= totalUnion;
      }

      if (totalScore < bestScore) {
        bestScore = totalScore;
        best = key;
      }
    }
    return best;
  }

  /**
   * Finds the position of the first value exceeding the supplied value, in a sorted list.
   * @param values The sorted list.
   * @param value The value to find the position of.
   * @return the position of the first value exceeding the supplied value.
   */
  private static int getPositionInList(Iterable<Long> values, long value) {
    Iterator<Long> it = values.iterator();
    int count = 0;
    while (it.hasNext()) {
      if (it.next() > value) {
        break;
      }
      count++;
    }
    return count;
  }

  /**
   * For each metric in the lookup table, get a sorted list of values as seen for devices' baseline
   * values.
   * @param lookup The device lookup table.
   * @return A dictionary of sorted values indexed by metric name.
   */
  private static Map<String, SortedSet<Long>> buildBaselineValuesTable(JSONObject lookup)
      throws JSONException {
    Map<String, SortedSet<Long>> table = new HashMap<>();
    Iterator<String> it = lookup.keys();
    while (it.hasNext()) {
      String device = it.next();
      JSONObject limits = lookup.getJSONObject(device);
      JSONObject prospectBaseline = limits.getJSONObject("baseline");
      Iterator<String> it2 = prospectBaseline.keys();
      while (it2.hasNext()) {
        String groupName = it2.next();
        JSONObject group = prospectBaseline.getJSONObject(groupName);
        Iterator<String> it3 = group.keys();
        while (it3.hasNext()) {
          String metric = it3.next();
          if (!table.containsKey(metric)) {
            table.put(metric, new TreeSet<Long>());
          }
          table.get(metric).add(group.getLong(metric));
        }
      }
    }
    return table;
  }
}
