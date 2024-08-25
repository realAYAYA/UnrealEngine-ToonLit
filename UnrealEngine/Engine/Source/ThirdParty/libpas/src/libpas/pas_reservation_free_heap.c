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

#include "pas_reservation_free_heap.h"

#include "pas_allocation_callbacks.h"
#include "pas_heap_kind.h"
#include "pas_heap_lock.h"
#include "pas_log.h"
#include "pas_page_malloc.h"
#include "pas_reservation.h"

pas_simple_large_free_heap pas_reservation_free_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER;

static pas_aligned_allocation_result reservation_source_allocate_aligned(size_t size,
                                                                         pas_alignment alignment,
                                                                         void* arg)
{
    PAS_ASSERT(!arg);
    return pas_reservation_try_allocate_without_deallocating_padding(size, alignment);
}

static void initialize_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = reservation_source_allocate_aligned;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

pas_allocation_result pas_reservation_free_heap_try_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    static const bool verbose = false;
    
    pas_large_free_heap_config config;
    pas_allocation_result result;

    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));
    
    pas_heap_lock_assert_held();

    if (verbose) {
        pas_log("%s: Doing reservation free heap allocation with size = %zu, alignment = %zu/%zu.\n",
                pas_heap_kind_get_string(pas_reservation_free_heap_kind), size, alignment.alignment,
                alignment.alignment_begin);
    }

    /* NOTE: This cannot align the size. That's because it cannot change the size. It has to
       use the size that the user passed. Anything else would result in us forever forgetting
       about that that alignment slop, since the caller will pass their original size when
       freeing the object later. */
    
    initialize_config(&config);
    result = pas_simple_large_free_heap_try_allocate(&pas_reservation_free_heap,
                                                     size, alignment,
                                                     &config);
	if (!result.did_succeed)
		return result;
	
    if (verbose)
        pas_log("Reservation allocated %p with size %zu\n", (void*)result.begin, size);

    pas_did_allocate(
        (void*)result.begin, size, pas_reservation_free_heap_kind, name, allocation_kind);
    
    return result;
}

pas_allocation_result pas_reservation_free_heap_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    pas_allocation_result result;
    result = pas_reservation_free_heap_try_allocate_with_alignment(size, alignment, name, allocation_kind);
    PAS_ASSERT(result.did_succeed);
    PAS_ASSERT(result.begin);
    return result;
}

#endif /* LIBPAS_ENABLED */

