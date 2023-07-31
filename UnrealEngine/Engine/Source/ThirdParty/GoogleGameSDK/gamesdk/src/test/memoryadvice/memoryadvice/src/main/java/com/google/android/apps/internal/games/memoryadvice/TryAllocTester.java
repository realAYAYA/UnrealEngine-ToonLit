package com.google.android.apps.internal.games.memoryadvice;

class TryAllocTester {
  static {
    System.loadLibrary("try-alloc-lib");
  }

  /**
   * Attempt to allocate memory on the native heap, then if successful immediately release it and
   * return 'true'. Otherwise return 'false'.
   * @param bytes The number of bytes to allocate.
   * @return 'true' if the allocation attempt was successful.
   */
  static native boolean tryAlloc(int bytes);
}
