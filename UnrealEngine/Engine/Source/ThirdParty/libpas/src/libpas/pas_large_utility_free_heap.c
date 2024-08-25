/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_large_utility_free_heap.h"

#include "pas_allocation_callbacks.h"
#include "pas_compute_summary_object_callbacks.h"
#include "pas_fast_large_free_heap.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_reservation.h"
#include "pas_reservation_free_heap.h"

pas_fast_large_free_heap pas_large_utility_free_heap = PAS_FAST_LARGE_FREE_HEAP_INITIALIZER;
size_t pas_large_utility_free_heap_num_allocated_object_bytes = 0;
size_t pas_large_utility_free_heap_num_allocated_object_bytes_peak = 0;

bool pas_large_utility_free_heap_talks_to_large_sharing_pool = true;

static const bool verbose = false;

static pas_aligned_allocation_result large_utility_aligned_allocator(size_t size,
                                                                     pas_alignment alignment,
                                                                     void* arg)
{
    size_t page_size;
    size_t aligned_size;
    pas_alignment aligned_alignment;
    pas_allocation_result allocation_result;
    pas_aligned_allocation_result result;

    PAS_ASSERT(!arg);

    /* This logic is kinda like pas_large_heap_physical_page_sharing_cache, except that this
       assumes that we have to manage the physical memory synchronously (holding the heap lock). */

    page_size = pas_page_malloc_alignment();
    aligned_size = pas_round_up_to_power_of_2(size, page_size);
    aligned_alignment = pas_alignment_round_up(alignment, page_size);

    pas_zero_memory(&result, sizeof(result));

    if (pas_large_utility_free_heap_talks_to_large_sharing_pool)
        pas_physical_page_sharing_pool_take_later(aligned_size);

    allocation_result = pas_reservation_free_heap_try_allocate_with_alignment(
        aligned_size, aligned_alignment,
        "pas_large_utility_free_heap/chunk",
        pas_delegate_allocation);

    if (!allocation_result.did_succeed) {
        pas_physical_page_sharing_pool_give_back(aligned_size);
        return result;
    }

    PAS_ASSERT(allocation_result.zero_mode == pas_zero_mode_is_all_zero);

    pas_reservation_commit((void*)allocation_result.begin, aligned_size);

    if (pas_large_utility_free_heap_talks_to_large_sharing_pool) {
        if (verbose) {
            pas_log("booting free %p...%p.\n",
                    (void*)allocation_result.begin,
                    (void*)(allocation_result.begin + aligned_size));
        }
        
        pas_large_sharing_pool_boot_free(
            pas_range_create(allocation_result.begin, allocation_result.begin + aligned_size),
            pas_physical_memory_is_locked_by_heap_lock,
            pas_may_mmap);
    }

    result.result = (void*)allocation_result.begin;
    result.result_size = size;
    result.left_padding = (void*)allocation_result.begin;
    result.left_padding_size = 0;
    result.right_padding = (char*)(void*)allocation_result.begin + size;
    result.right_padding_size = aligned_size - size;
    result.zero_mode = allocation_result.zero_mode;
    
    return result;
}

static void initialize_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = large_utility_aligned_allocator;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

void* pas_large_utility_free_heap_try_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name)
{
    pas_large_free_heap_config config;
    pas_allocation_result result;
    pas_heap_lock_assert_held();
    initialize_config(&config);
    alignment = pas_alignment_round_up(alignment, PAS_INTERNAL_MIN_ALIGN);
    result = pas_fast_large_free_heap_try_allocate(&pas_large_utility_free_heap, size, alignment, &config);
    pas_did_allocate(
        (void*)result.begin, size, pas_large_utility_free_heap_kind, name, pas_object_allocation);
    if (result.did_succeed) {
        if (pas_large_utility_free_heap_talks_to_large_sharing_pool) {
            bool commit_result;
            if (verbose) {
                pas_log("allocated and commit %p...%p.\n",
                        (void*)result.begin,
                        (void*)(result.begin + size));
            }
            commit_result = pas_large_sharing_pool_allocate_and_commit(
                pas_range_create(result.begin, result.begin + size),
                NULL,
                pas_physical_memory_is_locked_by_heap_lock,
                pas_may_mmap);
            PAS_ASSERT(commit_result);
        }
        pas_large_utility_free_heap_num_allocated_object_bytes += size;
        pas_large_utility_free_heap_num_allocated_object_bytes_peak = pas_max_uintptr(
            pas_large_utility_free_heap_num_allocated_object_bytes, pas_large_utility_free_heap_num_allocated_object_bytes_peak);
    }
    return (void*)result.begin;
}

void* pas_large_utility_free_heap_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name)
{
    void* result;
    result = pas_large_utility_free_heap_try_allocate_with_alignment(size, alignment, name);
    PAS_ASSERT(result || !size);
    return result;
}

void* pas_large_utility_free_heap_try_allocate(size_t size, const char* name)
{
    return pas_large_utility_free_heap_try_allocate_with_alignment(
        size, pas_alignment_create_trivial(), name);
}

void* pas_large_utility_free_heap_allocate(size_t size, const char* name)
{
    return pas_large_utility_free_heap_allocate_with_alignment(
        size, pas_alignment_create_trivial(), name);
}

void pas_large_utility_free_heap_deallocate(void* ptr, size_t size)
{
    pas_large_free_heap_config config;
    pas_heap_lock_assert_held();
    if (!size)
        return;
    initialize_config(&config);
    pas_will_deallocate(ptr, size, pas_large_utility_free_heap_kind, pas_object_allocation);
    if (pas_large_utility_free_heap_talks_to_large_sharing_pool) {
        if (verbose) {
            pas_log("freeing %p...%p.\n",
                    (void*)ptr,
                    (char*)ptr + size);
        }
        pas_large_sharing_pool_free(
            pas_range_create((uintptr_t)ptr, (uintptr_t)ptr + size),
            pas_physical_memory_is_locked_by_heap_lock,
            pas_may_mmap);
    }
    pas_fast_large_free_heap_deallocate(&pas_large_utility_free_heap,
                                        (uintptr_t)ptr, (uintptr_t)ptr + size,
                                        pas_zero_mode_may_have_non_zero,
                                        &config);
    pas_large_utility_free_heap_num_allocated_object_bytes -= size;
}

PAS_API pas_heap_summary pas_large_utility_free_heap_compute_summary(void)
{
    pas_heap_summary result;

    pas_heap_lock_assert_held();

    result = pas_heap_summary_create_empty();
    
    pas_fast_large_free_heap_for_each_free(
        &pas_large_utility_free_heap,
        pas_large_utility_free_heap_talks_to_large_sharing_pool
        ? pas_compute_summary_dead_object_callback
        : pas_compute_summary_dead_object_callback_without_physical_sharing,
        &result);

    result.allocated += pas_large_utility_free_heap_num_allocated_object_bytes;
    result.committed += pas_large_utility_free_heap_num_allocated_object_bytes;

    return result;
}

void* pas_large_utility_free_heap_allocate_for_allocation_config(
    size_t size, const char* name, pas_allocation_kind allocation_kind, void* arg)
{
    PAS_ASSERT(allocation_kind == pas_object_allocation);
    PAS_ASSERT(!arg);
    return pas_large_utility_free_heap_allocate(size, name);
}

void pas_large_utility_free_heap_deallocate_for_allocation_config(
    void* ptr, size_t size, pas_allocation_kind allocation_kind, void* arg)
{
    PAS_ASSERT(allocation_kind == pas_object_allocation);
    PAS_ASSERT(!arg);
    pas_large_utility_free_heap_deallocate(ptr, size);
}

const pas_allocation_config pas_large_utility_free_heap_allocation_config = {
    .allocate = pas_large_utility_free_heap_allocate_for_allocation_config,
    .deallocate = pas_large_utility_free_heap_deallocate_for_allocation_config,
    .arg = NULL
};

#endif /* LIBPAS_ENABLED */
