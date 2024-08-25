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

#include "pas_manually_decommittable_large_free_heap.h"

#include "pas_heap_lock.h"
#include "pas_log.h"
#include "pas_page_malloc.h"
#include "pas_scavenger.h"

void pas_manually_decommittable_large_free_heap_construct(pas_manually_decommittable_large_free_heap* heap)
{
    pas_simple_large_free_heap_construct(&heap->private_base);
    heap->is_committed = true;
    heap->clean_count = 0;
}

static void commit_impl_commit(uintptr_t begin, uintptr_t end, void* arg)
{
	static const bool verbose = false;
    PAS_ASSERT(!arg);
	if (verbose)
		pas_log("committing manually decommittable memory %p...%p.\n", (void*)begin, (void*)end);
    pas_page_malloc_commit((void*)begin, end - begin, pas_may_mmap);
}

static void commit_impl_decommit(uintptr_t begin, uintptr_t end, void* arg)
{
	static const bool verbose = false;
    PAS_ASSERT(!arg);
	if (verbose)
		pas_log("decommitting manually decommittable memory %p...%p.\n", (void*)begin, (void*)end);
    pas_page_malloc_decommit((void*)begin, end - begin, pas_may_mmap);
}

static void commit_impl_count(uintptr_t begin, uintptr_t end, void* arg)
{
    size_t* count;

    count = (size_t*)arg;

    (*count) += end - begin;
}

static size_t commit_impl(pas_manually_decommittable_large_free_heap* heap,
                          void (*commit)(uintptr_t begin, uintptr_t end, void* arg),
                          void* arg)
{
    size_t index;
    size_t result;

    pas_heap_lock_assert_held();

    result = 0;
    for (index = heap->private_base.free_list_size; index--;) {
        pas_large_free free_entry;
        uintptr_t begin;
        uintptr_t end;

        free_entry = heap->private_base.free_list[index];

        begin = pas_round_up_to_power_of_2(free_entry.begin, pas_page_malloc_alignment());
        end = pas_round_down_to_power_of_2(free_entry.end, pas_page_malloc_alignment());

        if (end > begin) {
            commit(begin, end, arg);
            result += end - begin;
        }
    }

    return result;
}

size_t pas_manually_decommittable_large_free_heap_commit(pas_manually_decommittable_large_free_heap* heap)
{
	static const bool verbose = false;
    size_t result;
    
    pas_heap_lock_assert_held();

	if (verbose)
		pas_log("may need to commit manually decommittable heap %p\n", heap);

    heap->clean_count = 0;
    
    if (heap->is_committed)
        return 0;

    result = commit_impl(heap, commit_impl_commit, NULL);
    heap->is_committed = true;
    pas_scavenger_did_create_eligible();
    return result;
}

size_t pas_manually_decommittable_large_free_heap_decommit(pas_manually_decommittable_large_free_heap* heap)
{
	static const bool verbose = false;
    size_t result;
    
    pas_heap_lock_assert_held();

	if (verbose)
		pas_log("decommitting manually decommittable heap %p\n", heap);
    
    if (!heap->is_committed)
        return 0;

    result = commit_impl(heap, commit_impl_decommit, NULL);
    heap->is_committed = false;
    return result;
}

bool pas_manually_decommittable_large_free_heap_scavenge_periodic(pas_manually_decommittable_large_free_heap* heap)
{
    pas_heap_lock_assert_held();
    
    if (!heap->is_committed)
        return false;

    PAS_ASSERT(heap->clean_count <= PAS_MANUALLY_DECOMMITTABLE_LARGE_FREE_HEAP_MAX_CLEAN_COUNT);
    
    if (heap->clean_count < PAS_MANUALLY_DECOMMITTABLE_LARGE_FREE_HEAP_MAX_CLEAN_COUNT) {
        heap->clean_count++;
        return true;
    }

    heap->clean_count = 0;
    pas_manually_decommittable_large_free_heap_decommit(heap);
    return false;
}

pas_allocation_result pas_manually_decommittable_large_free_heap_try_allocate(pas_manually_decommittable_large_free_heap* heap,
                                                                              size_t size,
                                                                              pas_alignment alignment,
                                                                              pas_large_free_heap_config* config)
{
    PAS_ASSERT(heap->is_committed);
    return pas_simple_large_free_heap_try_allocate(&heap->private_base, size, alignment, config);
}

void pas_manually_decommittable_large_free_heap_deallocate(pas_manually_decommittable_large_free_heap* heap,
                                                           uintptr_t begin,
                                                           uintptr_t end,
                                                           pas_zero_mode zero_mode,
                                                           pas_large_free_heap_config* config)
{
    PAS_ASSERT(heap->is_committed);
    pas_simple_large_free_heap_deallocate(&heap->private_base, begin, end, zero_mode, config);
}

size_t pas_manually_decommittable_large_free_heap_get_num_decommitted_bytes(pas_manually_decommittable_large_free_heap* heap)
{
    size_t result;

    pas_heap_lock_assert_held();

    result = 0;

    if (!heap->is_committed)
        commit_impl(heap, commit_impl_count, &result);

    return result;
}

#endif /* LIBPAS_ENABLED */

