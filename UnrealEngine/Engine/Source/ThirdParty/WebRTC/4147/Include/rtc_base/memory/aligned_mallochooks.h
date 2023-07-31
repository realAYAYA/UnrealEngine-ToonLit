/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MEMORY_ALIGNED_MALLOCHOOKS_H_
#define RTC_BASE_MEMORY_ALIGNED_MALLOCHOOKS_H_


namespace webrtc {

void base_memory_set_custom_hooks(void*(*alloc_hook)(size_t), void(*free_hook)(void*));

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_ALIGNED_MALLOCHOOKS_H_
