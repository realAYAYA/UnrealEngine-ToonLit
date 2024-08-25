/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

PAS_BEGIN_EXTERN_C;

#pragma mark - Allocator functions

PAS_API void* __thingy_try_allocate_primitive(size_t size);
PAS_API void* __thingy_try_allocate_primitive_zeroed(size_t size);
PAS_API void* __thingy_try_allocate_primitive_with_alignment(size_t size, size_t alignment);

PAS_API void* __thingy_try_reallocate_primitive(
    void* old_ptr, size_t new_size);

#if defined(__GNUC__)
__attribute__((malloc))
#endif
PAS_API void* __thingy_try_allocate(pas_heap_ref* heap_ref);

PAS_API void* __thingy_try_allocate_zeroed(pas_heap_ref* heap_ref);

/* FIXME: This should take the size, since the caller calculates it anyway. */
PAS_API void* __thingy_try_allocate_array(pas_heap_ref* heap_ref,
                                          size_t count,
                                          size_t alignment);

PAS_API void* __thingy_try_allocate_zeroed_array(pas_heap_ref* heap_ref,
                                                 size_t count,
                                                 size_t alignment);

PAS_API void* __thingy_try_reallocate_array(void* old_ptr,
                                            pas_heap_ref* heap_ref,
                                            size_t new_count);

PAS_API void __thingy_deallocate(void*);

PAS_END_EXTERN_C;

