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

#ifndef PAS_SIMPLE_DECOMMITTABLE_LARGE_FREE_HEAP_H
#define PAS_SIMPLE_DECOMMITTABLE_LARGE_FREE_HEAP_H

#include "pas_manually_decommittable_large_free_heap.h"
#include "pas_page_sharing_participant.h"

PAS_BEGIN_EXTERN_C;

struct pas_simple_decommittable_large_free_heap;
typedef struct pas_simple_decommittable_large_free_heap pas_simple_decommittable_large_free_heap;

struct pas_simple_decommittable_large_free_heap {
    pas_manually_decommittable_large_free_heap private_base;
    pas_page_sharing_participant_payload_with_use_epoch sharing_payload;
    bool is_added_to_sharing_pool;
};

#define PAS_SIMPLE_DECOMMITTABLE_LARGE_FREE_HEAP_INITIALIZER { \
        .private_base = PAS_MANUALLY_DECOMMITTABLE_LARGE_FREE_HEAP_INITIALIZER, \
        .sharing_payload = PAS_PAGE_SHARING_PARTICIPANT_PAYLOAD_WITH_USE_EPOCH_INITIALIZER, \
        .is_added_to_sharing_pool = false \
    }

PAS_API void pas_simple_decommittable_large_free_heap_construct(pas_simple_decommittable_large_free_heap* heap);

PAS_API size_t pas_simple_decommittable_large_free_heap_commit(pas_simple_decommittable_large_free_heap* heap);

static inline pas_page_sharing_participant_payload*
pas_simple_decommittable_large_free_heap_get_sharing_payload(pas_simple_decommittable_large_free_heap* heap)
{
    return &heap->sharing_payload.base;
}

static inline uint64_t pas_simple_decommittable_large_free_heap_get_use_epoch(pas_simple_decommittable_large_free_heap* heap)
{
    return heap->sharing_payload.use_epoch;
}

PAS_API bool pas_simple_decommittable_large_free_heap_is_eligible(pas_simple_decommittable_large_free_heap* heap);
PAS_API pas_page_sharing_pool_take_result pas_simple_decommittable_large_free_heap_decommit(pas_simple_decommittable_large_free_heap* heap,
                                                                                            pas_deferred_decommit_log* decommit_log);

PAS_API pas_allocation_result
pas_simple_decommittable_large_free_heap_try_allocate(pas_simple_decommittable_large_free_heap* heap,
                                                      size_t size,
                                                      pas_alignment alignment,
                                                      pas_large_free_heap_config* config);

PAS_API void pas_simple_decommittable_large_free_heap_deallocate(pas_simple_decommittable_large_free_heap* heap,
                                                                 uintptr_t begin,
                                                                 uintptr_t end,
                                                                 pas_zero_mode zero_mode,
                                                                 pas_large_free_heap_config* config);

static inline size_t pas_simple_decommittable_large_free_heap_get_num_free_bytes(pas_simple_decommittable_large_free_heap* heap)
{
    return pas_manually_decommittable_large_free_heap_get_num_free_bytes(&heap->private_base);
}


PAS_END_EXTERN_C;

#endif /* PAS_SIMPLE_DECOMMITTABLE_LARGE_FREE_HEAP_H */

