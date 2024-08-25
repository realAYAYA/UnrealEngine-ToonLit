/*
 * Copyright Epic Games, Inc. All Rights Reserved.
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

#ifndef PAS_RESERVE_COMMIT_CACHE_LARGE_FREE_HEAP_H
#define PAS_RESERVE_COMMIT_CACHE_LARGE_FREE_HEAP_H

#include "pas_simple_decommittable_large_free_heap.h"

PAS_BEGIN_EXTERN_C;

struct pas_reserve_commit_cache_large_free_heap;
typedef struct pas_reserve_commit_cache_large_free_heap pas_reserve_commit_cache_large_free_heap;

struct pas_reserve_commit_cache_large_free_heap {
    pas_simple_decommittable_large_free_heap private_base;
};

#define PAS_RESERVE_COMMIT_CACHE_LARGE_FREE_HEAP_INITIALIZER { .private_base = PAS_SIMPLE_DECOMMITTABLE_LARGE_FREE_HEAP_INITIALIZER }

PAS_API void pas_reserve_commit_cache_large_free_heap_construct(pas_reserve_commit_cache_large_free_heap* heap);
PAS_API pas_allocation_result pas_reserve_commit_cache_large_free_heap_try_allocate(pas_reserve_commit_cache_large_free_heap* heap,
                                                                                    size_t size,
                                                                                    pas_alignment alignment,
                                                                                    pas_large_free_heap_config* config);
PAS_API void pas_reserve_commit_cache_large_free_heap_deallocate(pas_reserve_commit_cache_large_free_heap* heap,
                                                                 uintptr_t begin,
                                                                 uintptr_t end,
                                                                 pas_zero_mode zero_mode,
                                                                 pas_large_free_heap_config* config);

PAS_END_EXTERN_C;

#endif /* PAS_RESERVE_COMMIT_CACHE_LARGE_FREE_HEAP_H */

