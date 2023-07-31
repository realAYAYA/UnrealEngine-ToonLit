/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_MEMORY_FUNCTIONHOOKS_H_
#define RTC_BASE_MEMORY_FUNCTIONHOOKS_H_

#include <stdint.h>
#include <stddef.h>

namespace rtc {

struct MemoryHookFunctions {
  void* (*AllocFN)(size_t Bytes, size_t Alignment) = nullptr;
  void* (*ReallocFN)(void* Ptr, size_t Bytes, size_t Alignment) = nullptr;
  void  (*FreeFN)(void* Ptr) = nullptr;
};

void SetMemoryAllocationFunctions(const MemoryHookFunctions& MemHookFunctions);

}  // namespace rtc

#endif  // RTC_BASE_MEMORY_FUNCTIONHOOKS_H_
