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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_simple_decommittable_large_free_heap.h"

#include "pas_deferred_decommit_log.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_page_sharing_pool.h"

void pas_simple_decommittable_large_free_heap_construct(pas_simple_decommittable_large_free_heap* heap)
{
    pas_heap_lock_assert_held();
    pas_manually_decommittable_large_free_heap_construct(&heap->private_base);
    pas_page_sharing_participant_payload_with_use_epoch_construct(&heap->sharing_payload);
    heap->is_added_to_sharing_pool = false;
}

size_t pas_simple_decommittable_large_free_heap_commit(pas_simple_decommittable_large_free_heap* heap)
{
    size_t result;
    pas_heap_lock_assert_held();
    result = pas_manually_decommittable_large_free_heap_commit(&heap->private_base);

    if (!heap->is_added_to_sharing_pool) {
        pas_page_sharing_pool_add(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                heap, pas_page_sharing_participant_simple_decommittable_large_free_heap));
        heap->is_added_to_sharing_pool = true;
    }
    
    heap->sharing_payload.use_epoch = pas_get_epoch();
    pas_page_sharing_pool_did_create_delta(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            heap, pas_page_sharing_participant_simple_decommittable_large_free_heap));
    return result;
}

bool pas_simple_decommittable_large_free_heap_is_eligible(pas_simple_decommittable_large_free_heap* heap)
{
    return heap->private_base.is_committed;
}

pas_page_sharing_pool_take_result pas_simple_decommittable_large_free_heap_decommit(pas_simple_decommittable_large_free_heap* heap,
                                                                                    pas_deferred_decommit_log* decommit_log)
{
    size_t bytes_decommitted;

    pas_heap_lock_assert_held();

    PAS_ASSERT(heap->is_added_to_sharing_pool);

    bytes_decommitted = pas_manually_decommittable_large_free_heap_decommit(&heap->private_base);
    decommit_log->total += bytes_decommitted;

    pas_page_sharing_pool_did_create_delta(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            heap, pas_page_sharing_participant_simple_decommittable_large_free_heap));

    if (bytes_decommitted)
        return pas_page_sharing_pool_take_success;
    return pas_page_sharing_pool_take_none_available;
}

pas_allocation_result
pas_simple_decommittable_large_free_heap_try_allocate(pas_simple_decommittable_large_free_heap* heap,
                                                      size_t size,
                                                      pas_alignment alignment,
                                                      pas_large_free_heap_config* config)
{
    pas_simple_decommittable_large_free_heap_commit(heap);
    return pas_manually_decommittable_large_free_heap_try_allocate(&heap->private_base, size, alignment, config);
}

void pas_simple_decommittable_large_free_heap_deallocate(pas_simple_decommittable_large_free_heap* heap,
                                                         uintptr_t begin,
                                                         uintptr_t end,
                                                         pas_zero_mode zero_mode,
                                                         pas_large_free_heap_config* config)
{
    pas_simple_decommittable_large_free_heap_commit(heap);
    pas_manually_decommittable_large_free_heap_deallocate(&heap->private_base, begin, end, zero_mode, config);
}

#endif /* LIBPAS_ENABLED */

