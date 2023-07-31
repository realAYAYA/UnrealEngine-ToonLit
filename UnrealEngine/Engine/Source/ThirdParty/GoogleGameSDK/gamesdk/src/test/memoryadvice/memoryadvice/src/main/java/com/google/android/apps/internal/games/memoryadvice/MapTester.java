package com.google.android.apps.internal.games.memoryadvice;

import android.util.Log;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

/**
 * A class to use a small memory mapped file as a memory warning canary. When the file has been
 * unloaded it is a signal that memory is running low on the device.
 */
class MapTester {
  private static final String TAG = MapTester.class.getSimpleName();

  private MappedByteBuffer map;

  /**
   * Make a new map tester.
   * @param directory The (ideally temporary) folder in which the file should be created.
   */
  MapTester(File directory) {
    try {
      File mapFile = File.createTempFile("mapped", ".txt", directory);

      try (FileWriter out = new FileWriter(mapFile);
           BufferedWriter writer = new BufferedWriter(out)) {
        writer.write("_");
      }

      try (FileInputStream stream = new FileInputStream(mapFile);
           FileChannel channel = stream.getChannel()) {
        map = channel.map(FileChannel.MapMode.READ_ONLY, 0, channel.size());
      }

      reset();
    } catch (IOException ex) {
      Log.w(TAG, ex);
    }
  }

  void reset() {
    if (map != null) {
      map.load();
    }
  }

  boolean warning() {
    if (map == null) {
      return false;
    }
    return !map.isLoaded();
  }
}
