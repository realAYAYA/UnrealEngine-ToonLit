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

#include "pas_global_physical_page_sharing_cache.h"

#include "pas_allocation_callbacks.h"
#include "pas_enumerable_page_malloc.h"
#include "pas_heap_kind.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_log.h"
#include "pas_page_sharing_pool.h"
#include "pas_physical_memory_transaction.h"
#include "pas_reservation.h"
#include "pas_reservation_free_heap.h"

pas_simple_large_free_heap pas_global_physical_page_sharing_cache = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER;

static pas_aligned_allocation_result global_physical_allocate_aligned(size_t size,
																	  pas_alignment alignment,
																	  void* arg)
{
	pas_aligned_allocation_result result;

	PAS_ASSERT(pas_reservation_commit_mode == pas_decommitted);

	result = pas_enumerable_page_malloc_try_allocate_without_deallocating_padding(size, alignment, pas_committed);

	if (!result.result)
		return result;

    pas_physical_page_sharing_pool_take_later(pas_aligned_allocation_result_total_size(result));

	PAS_ASSERT(result.zero_mode == pas_zero_mode_is_all_zero);

	pas_large_sharing_pool_boot_free(
		pas_range_create((uintptr_t)result.left_padding, (uintptr_t)result.right_padding + result.right_padding_size),
		pas_physical_memory_is_locked_by_virtual_range_common_lock,
		pas_may_mmap);

	return result;
}

static void initialize_config(pas_large_free_heap_config* config)
{
	PAS_ASSERT(pas_reservation_commit_mode == pas_decommitted);
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = global_physical_allocate_aligned;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

pas_allocation_result pas_global_physical_page_sharing_cache_try_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name)
{
	pas_allocation_result result;
    pas_large_free_heap_config config;

	pas_heap_lock_assert_held();
	
	if (pas_reservation_commit_mode == pas_committed) {
		result = pas_reservation_free_heap_try_allocate_with_alignment(size, alignment, name, pas_delegate_allocation);
		if (!result.did_succeed)
			return result;

		pas_physical_page_sharing_pool_take_later(size);
		
		pas_large_sharing_pool_boot_free(
			pas_range_create(result.begin, result.begin + size),
			pas_physical_memory_is_locked_by_virtual_range_common_lock,
			pas_may_mmap);

		return result;
	}

	PAS_ASSERT(pas_reservation_commit_mode == pas_decommitted);

	initialize_config(&config);
	result = pas_simple_large_free_heap_try_allocate(&pas_global_physical_page_sharing_cache, size, alignment, &config);
	if (!result.did_succeed)
		return result;

	pas_large_sharing_pool_testing_assert_booted_and_free(
		pas_range_create(result.begin, result.begin + size),
		pas_physical_memory_is_locked_by_virtual_range_common_lock,
		pas_may_mmap);

	pas_did_allocate((void*)result.begin, size, pas_global_physical_page_sharing_cache_kind, name, pas_delegate_allocation);

	return result;
}

pas_allocation_result pas_global_physical_page_sharing_cache_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name)
{
	pas_allocation_result result;
	result = pas_global_physical_page_sharing_cache_try_allocate_with_alignment(size, alignment, name);
	PAS_ASSERT(result.did_succeed);
	PAS_ASSERT(result.begin);
	return result;
}

pas_allocation_result pas_global_physical_page_sharing_cache_try_allocate_committed_with_alignment(
	size_t size, pas_alignment alignment, const char* name, pas_physical_memory_transaction* transaction)
{
	pas_allocation_result result;
    pas_large_free_heap_config config;
	
	if (pas_reservation_commit_mode == pas_committed)
		return pas_reservation_free_heap_try_allocate_with_alignment(size, alignment, name, pas_delegate_allocation);

	PAS_ASSERT(transaction);

	initialize_config(&config);
	result = pas_simple_large_free_heap_try_allocate(&pas_global_physical_page_sharing_cache, size, alignment, &config);
	if (!result.did_succeed)
		return result;

	pas_large_sharing_pool_testing_assert_booted_and_free(
		pas_range_create(result.begin, result.begin + size),
		pas_physical_memory_is_locked_by_virtual_range_common_lock,
		pas_may_mmap);

	if (!pas_large_sharing_pool_allocate_and_commit(
			pas_range_create(result.begin, result.begin + size),
			transaction, pas_physical_memory_is_locked_by_virtual_range_common_lock, pas_may_mmap)) {
		PAS_ASSERT(pas_physical_memory_transaction_is_aborting(transaction));
		pas_simple_large_free_heap_deallocate(
			&pas_global_physical_page_sharing_cache, result.begin, result.begin + size, result.zero_mode, &config);
		return pas_allocation_result_create_failure();
	}

	pas_did_allocate((void*)result.begin, size, pas_global_physical_page_sharing_cache_kind, name, pas_delegate_allocation);

	return result;
}

pas_allocation_result pas_global_physical_page_sharing_cache_allocate_committed_with_alignment(
	size_t size, pas_alignment alignment, const char* name, pas_physical_memory_transaction* transaction)
{
	pas_allocation_result result;

	if (pas_reservation_commit_mode == pas_committed)
		return pas_reservation_free_heap_allocate_with_alignment(size, alignment, name, pas_delegate_allocation);

	result = pas_global_physical_page_sharing_cache_try_allocate_committed_with_alignment(size, alignment, name, transaction);
	if (pas_physical_memory_transaction_is_aborting(transaction))
		PAS_ASSERT(!result.did_succeed);
	else
		PAS_ASSERT(result.did_succeed);
	return result;
}

pas_allocation_result pas_global_physical_page_sharing_cache_provider(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
	pas_primordial_page_state desired_state,
    void *arg)
{
	PAS_UNUSED_PARAM(heap);
	PAS_ASSERT(!arg);
	switch (desired_state) {
	case pas_primordial_page_is_shared:
		return pas_global_physical_page_sharing_cache_try_allocate_with_alignment(size, alignment, name);
	case pas_primordial_page_is_committed:
		return pas_global_physical_page_sharing_cache_try_allocate_committed_with_alignment(size, alignment, name, transaction);
	default:
		PAS_ASSERT(!"Should not be reached");
		return pas_allocation_result_create_failure();
	}
}

#endif /* LIBPAS_ENABLED */

