/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_THREAD_OBSERVER_H_
#define RTC_BASE_THREAD_OBSERVER_H_

#include "rtc_base/system/rtc_export.h"
#include "rtc_base/constructor_magic.h"

// enables rtc extension functionality
#ifndef WEBRTC_EXTENSION_THREAD_OBSERVER
#define WEBRTC_EXTENSION_THREAD_OBSERVER 1
#endif  // WEBRTC_EXTENSION_THREAD_OBSERVER

namespace rtc {
#if WEBRTC_EXTENSION_THREAD_OBSERVER

class Thread;
class PlatformThread;

class RTC_EXPORT ThreadObserver {
 public:
  virtual void OnAddThread(const Thread* thread) = 0;
  virtual void OnRemoveThread(const Thread* thread) = 0;
  virtual void OnAddThread(const PlatformThread* platform_thread) = 0;
  virtual void OnRemoveThread(const PlatformThread* platform_thread) = 0;

 protected:
  virtual ~ThreadObserver() = default;
};

class ThreadListener {
 public:
  // Singleton, constructor and destructor are private.
  static ThreadListener& Instance();
  // Used to receive events from thread lifetime. Only one observer can be
  // registered at a time. UnregisterObserver should be called before the
  // observer object is destroyed.
  //
  // Expects to be invoked in the same thread without any actual thread manipulations
  // (i.e. when the actual thread number == 0)
  void RegisterObserver(ThreadObserver* observer);
  void UnregisterObserver();
  //

  static void NotifyAddThread(const Thread* thread);
  static void NotifyRemoveThread(const Thread* thread);
  static void NotifyAddThread(const PlatformThread* platform_thread);
  static void NotifyRemoveThread(const PlatformThread* platform_thread);

 private:
  ThreadListener() = default;
  ~ThreadListener() = default;

  void NotifyAddThreadInternal(const Thread* thread) const;
  void NotifyRemoveThreadInternal(const Thread* thread) const;
  void NotifyAddThreadInternal(const PlatformThread* platform_thread) const;
  void NotifyRemoveThreadInternal(const PlatformThread* platform_thread) const;

  ThreadObserver* observer_{nullptr};

  RTC_DISALLOW_COPY_AND_ASSIGN(ThreadListener);
};

#endif  // WEBRTC_EXTENSION_THREAD_OBSERVER
}  // namespace rtc

#endif  // RTC_BASE_THREAD_OBSERVER_H_
