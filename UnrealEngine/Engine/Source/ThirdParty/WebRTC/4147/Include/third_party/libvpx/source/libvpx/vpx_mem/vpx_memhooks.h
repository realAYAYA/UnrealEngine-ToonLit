/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VPX_MEM_VPX_MEMHOOKS_H_
#define VPX_VPX_MEM_VPX_MEMHOOKS_H_


#if defined(__cplusplus)
extern "C" {
#endif

void vpx_memory_set_custom_hooks(void*(*alloc_hook)(size_t), void(*free_hook)(void*));

#if defined(__cplusplus)
}
#endif

#endif  // VPX_VPX_MEM_VPX_MEMHOOKS_H_
