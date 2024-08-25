/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_compact_heap_reservation.h"

#include "pas_page_malloc.h"
#include "pas_reservation.h"

PAS_DEFINE_LOCK(pas_compact_heap_reservation);

size_t pas_compact_heap_reservation_size =
    (size_t)1 << PAS_COMPACT_PTR_BITS << PAS_INTERNAL_MIN_ALIGN_SHIFT;
uintptr_t pas_compact_heap_reservation_base = 0;
size_t pas_compact_heap_reservation_bump = 0;

pas_aligned_allocation_result pas_compact_heap_reservation_try_allocate(size_t size, size_t alignment)
{
    pas_aligned_allocation_result result;
    uintptr_t reservation_end;
    uintptr_t padding_start;
    uintptr_t allocation_start;
    uintptr_t allocation_end;
    size_t allocation_size;
    
    pas_compact_heap_reservation_lock_lock();

    allocation_size = pas_round_up_to_power_of_2(size, pas_page_malloc_alignment());
    
    if (!pas_compact_heap_reservation_base) {
        pas_aligned_allocation_result page_result;

        PAS_ASSERT(pas_is_aligned(pas_compact_heap_reservation_size, pas_page_malloc_alignment()));

        page_result = pas_page_malloc_try_allocate_without_deallocating_padding(
            pas_compact_heap_reservation_size, pas_alignment_create_trivial(), pas_reservation_commit_mode);
        PAS_ASSERT(!page_result.left_padding_size);
        PAS_ASSERT(!page_result.right_padding_size);
        PAS_ASSERT(page_result.result);
        PAS_ASSERT(page_result.result_size == pas_compact_heap_reservation_size);

        pas_compact_heap_reservation_base = (uintptr_t)page_result.result;
        pas_compact_heap_reservation_bump = pas_page_malloc_alignment(); /* Create a zero page. */

        PAS_ASSERT(pas_is_aligned(pas_compact_heap_reservation_base, pas_page_malloc_alignment()));
    }

    PAS_ASSERT(pas_is_aligned(pas_compact_heap_reservation_base, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(pas_compact_heap_reservation_bump, pas_page_malloc_alignment()));

    reservation_end = pas_compact_heap_reservation_base + pas_compact_heap_reservation_size;
    padding_start = pas_compact_heap_reservation_base + pas_compact_heap_reservation_bump;
    allocation_start = pas_round_up_to_power_of_2(padding_start, alignment);

    PAS_ASSERT(pas_is_aligned(reservation_end, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(padding_start, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(allocation_start, pas_page_malloc_alignment()));

    if (allocation_start > reservation_end
        || allocation_start < padding_start
        || reservation_end - allocation_start < allocation_size) {
		pas_compact_heap_reservation_lock_unlock();
        return pas_aligned_allocation_result_create_empty();
	}

    allocation_end = allocation_start + allocation_size;

    PAS_ASSERT(pas_is_aligned(allocation_end, pas_page_malloc_alignment()));

    pas_compact_heap_reservation_bump = allocation_end - pas_compact_heap_reservation_base;
    
    PAS_ASSERT(pas_is_aligned(pas_compact_heap_reservation_bump, pas_page_malloc_alignment()));

    result.left_padding = (void*)padding_start;
    result.left_padding_size = allocation_start - padding_start;
    result.result = (void*)allocation_start;
    result.result_size = size;
    result.right_padding = (void*)(allocation_start + size);
    result.right_padding_size = allocation_size - size;
    result.zero_mode = pas_zero_mode_is_all_zero;
	
	pas_compact_heap_reservation_lock_unlock();

    PAS_ASSERT(pas_is_aligned((uintptr_t)result.left_padding, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(result.left_padding_size, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned((uintptr_t)result.result, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(result.result_size + result.right_padding_size, pas_page_malloc_alignment()));
	
	return result;
}

#endif /* LIBPAS_ENABLED */
