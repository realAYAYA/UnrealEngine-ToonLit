/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "pas_immortal_heap.h"

#include "pas_allocation_callbacks.h"
#include "pas_compact_heap_reservation.h"
#include "pas_lock.h"
#include "pas_page_malloc.h"
#include "pas_reservation.h"
#include "pas_scavenger.h"

PAS_DEFINE_LOCK(pas_immortal_heap);

uintptr_t pas_immortal_heap_current;
uintptr_t pas_immortal_heap_end;
size_t pas_immortal_heap_allocated_external = 0;
size_t pas_immortal_heap_allocated_internal = 0;
size_t pas_immortal_heap_allocation_granule = 65536;
bool pas_immortal_heap_is_committed = true;
uint8_t pas_immortal_heap_clean_count = 0;

static bool bump_is_ok(uintptr_t bump,
                       size_t size)
{
    return bump <= pas_immortal_heap_end
        && bump >= pas_immortal_heap_current
        && pas_immortal_heap_end - bump >= size;
}

static size_t commit_slack_impl(void (*commit)(void*, size_t, pas_mmap_capability))
{
    static const bool verbose = false;
    
    uintptr_t begin;
    uintptr_t end;

    pas_immortal_heap_lock_assert_held();

    begin = pas_round_up_to_power_of_2(pas_immortal_heap_current, pas_page_malloc_alignment());
    end = pas_immortal_heap_end;

    PAS_ASSERT(pas_is_aligned(begin, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(end, pas_page_malloc_alignment()));

    PAS_ASSERT(begin <= end);

    if (begin < end) {
        if (verbose) {
            pas_log("pas_immortal_heap doing %p (%s) on %p...%p.\n",
                    commit,
                    commit == pas_page_malloc_commit ? "commit" : commit == pas_page_malloc_decommit ? "decommit" : "other",
                    (void*)begin, (void*)end);
        }
        commit((void*)begin, end - begin, pas_may_mmap);
    }

    return end - begin;
}

static size_t commit_slack(void)
{
    return commit_slack_impl(pas_page_malloc_commit);
}

static size_t decommit_slack(void)
{
    return commit_slack_impl(pas_page_malloc_decommit);
}

void* pas_immortal_heap_allocate_with_manual_alignment(size_t size,
                                                       size_t alignment,
                                                       const char* name,
                                                       pas_allocation_kind allocation_kind)
{
    static const unsigned verbose = 0;
    
    uintptr_t aligned_bump;

    pas_immortal_heap_lock_lock();

    pas_immortal_heap_clean_count = 0;

    aligned_bump = pas_round_up_to_power_of_2(pas_immortal_heap_current, alignment);
    if (!bump_is_ok(aligned_bump, size)) {
        size_t allocation_size;
        pas_aligned_allocation_result allocation_result;

        if (verbose)
            pas_log("pas_immortal_heap allocating new cache.\n");

        if (pas_immortal_heap_is_committed)
            decommit_slack();

        allocation_size = pas_round_up_to_power_of_2(pas_max_uintptr(size, pas_immortal_heap_allocation_granule), pas_page_malloc_alignment());

        allocation_result = pas_compact_heap_reservation_try_allocate(allocation_size, alignment);
        PAS_ASSERT(allocation_result.result);
        PAS_ASSERT(allocation_result.result_size == allocation_size);
        PAS_ASSERT(!allocation_result.right_padding_size);

        pas_reservation_commit(allocation_result.result, allocation_size);
        
        pas_immortal_heap_current = (uintptr_t)allocation_result.result;
        pas_immortal_heap_end = pas_immortal_heap_current + allocation_size;

        if (verbose)
            pas_log("pas_immortal_heap new cache is at %p...%p.\n", (void*)pas_immortal_heap_current, (void*)pas_immortal_heap_end);

        pas_immortal_heap_allocated_external += allocation_size;

        pas_immortal_heap_is_committed = true;
        pas_scavenger_did_create_eligible();

        aligned_bump = pas_immortal_heap_current;

        PAS_ASSERT(bump_is_ok(aligned_bump, size));
        PAS_ASSERT(pas_is_aligned(aligned_bump, alignment));
    } else if (!pas_immortal_heap_is_committed) {
        if (verbose)
            pas_log("pas_immortal_heap recommitting slack with current = %p.\n", (void*)pas_immortal_heap_current);
        commit_slack();
        pas_immortal_heap_is_committed = true;
        pas_scavenger_did_create_eligible();
    }

    pas_immortal_heap_current = aligned_bump + size;

    pas_did_allocate((void*)aligned_bump, size, pas_immortal_heap_kind, name, allocation_kind);
    pas_immortal_heap_allocated_internal += size;

    if (verbose) {
        pas_log("pas_immortal_heap allocated %zu for %s at %p.\n", size, name, (void*)aligned_bump);
        if (verbose >= 2) {
            pas_log("immortal heap internal size: %zu.\n", pas_immortal_heap_allocated_internal);
            pas_log("immortal heap external size: %zu.\n", pas_immortal_heap_allocated_external);
        }
    }

    pas_immortal_heap_lock_unlock();
    
    return (void*)aligned_bump;
}

void* pas_immortal_heap_allocate_with_alignment(size_t size,
                                                size_t alignment,
                                                const char* name,
                                                pas_allocation_kind allocation_kind)
{
    static const bool verbose = false;
    void* result;
    result = pas_immortal_heap_allocate_with_manual_alignment(
        size, pas_max_uintptr(alignment, PAS_INTERNAL_MIN_ALIGN), name, allocation_kind);
    if (verbose)
        pas_log("immortal allocated = %p.\n", result);
    PAS_ASSERT(pas_is_aligned((uintptr_t)result, PAS_INTERNAL_MIN_ALIGN));
    return result;
}

void* pas_immortal_heap_allocate(size_t size,
                                 const char* name,
                                 pas_allocation_kind allocation_kind)
{
    return pas_immortal_heap_allocate_with_alignment(size, 1, name, allocation_kind);
}

size_t pas_immortal_heap_decommit(void)
{
    size_t result;
    pas_immortal_heap_lock_lock();
    if (pas_immortal_heap_is_committed) {
        result = decommit_slack();
        pas_immortal_heap_is_committed = false;
    } else
        result = 0;
    pas_immortal_heap_lock_unlock();
    return result;
}

bool pas_immortal_heap_scavenge_periodic(void)
{
    bool result;
    pas_immortal_heap_lock_lock();
    if (!pas_immortal_heap_is_committed)
        result = false;
    else if (pas_immortal_heap_clean_count < PAS_IMMORTAL_HEAP_MAX_CLEAN_COUNT) {
        pas_immortal_heap_clean_count++;
        result = true;
    } else {
        decommit_slack();
        pas_immortal_heap_is_committed = false;
        result = false;
        pas_immortal_heap_clean_count = 0;
    }
    pas_immortal_heap_lock_unlock();
    return result;
}

#endif /* LIBPAS_ENABLED */
