// This file provides the native functions for the TryAllocTester.

#include <jni.h>
#include <stdlib.h>

extern "C" JNIEXPORT jboolean JNICALL
Java_com_google_android_apps_internal_games_memoryadvice_TryAllocTester_tryAlloc(
    JNIEnv *env, jclass clazz, jint bytes) {
  size_t byte_count = (size_t)bytes;
  char *data = (char *)malloc(byte_count);
  if (data) {
    free(data);
    return true;
  }
  return false;
}
