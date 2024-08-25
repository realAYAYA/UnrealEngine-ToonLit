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

#include "inline_medium_page_header_config.h"

#if PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER

#include "pas_global_physical_page_sharing_cache.h"
#include "pas_heap_config_inlines.h"
#include "pas_segregated_page_config_inlines.h"

const pas_heap_config inline_medium_page_header_config = INLINE_MEDIUM_PAGE_HEADER_CONFIG;

pas_heap_runtime_config inline_medium_page_header_runtime_config = {
    .sharing_mode = pas_share_pages,
    .statically_allocated = true,
    .is_part_of_heap = true,
    .directory_size_bound_for_partial_views = 0,
    .directory_size_bound_for_baseline_allocators = 0,
    .directory_size_bound_for_no_view_cache = 0,
    .max_segregated_object_size = UINT_MAX,
    .max_bitfit_object_size = UINT_MAX,
    .view_cache_capacity_for_object_size = pas_heap_runtime_config_zero_view_cache_capacity
};

pas_page_base* inline_medium_page_header_config_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary)
{
	PAS_UNUSED_PARAM(enumerator);
	return (pas_page_base*)boundary;
}

void* inline_medium_page_header_config_allocate_page(pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role)
{
	PAS_ASSERT(role == pas_segregated_page_exclusive_role);
	return (void*)pas_global_physical_page_sharing_cache_try_allocate_committed_with_alignment(
		INLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, pas_alignment_create_traditional(INLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE),
		"inline_medium_page_header_config/page", transaction).begin;
}

pas_page_base* inline_medium_page_header_config_create_page_header(void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode)
{
	PAS_ASSERT(kind == pas_small_exclusive_segregated_page_kind);
	PAS_UNUSED_PARAM(heap_lock_hold_mode);
	return (pas_page_base*)boundary;
}

void inline_medium_page_header_config_destroy_page_header(pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode)
{
	PAS_UNUSED_PARAM(heap_lock_hold_mode);
	PAS_UNUSED_PARAM(page);
}

pas_segregated_shared_page_directory* inline_medium_page_header_config_shared_page_directory_selector(
	pas_segregated_heap* heap, pas_segregated_size_directory* directory)
{
	PAS_ASSERT(!"Should not be reached");
	PAS_UNUSED_PARAM(heap);
	PAS_UNUSED_PARAM(directory);
	return NULL;
}

pas_aligned_allocation_result inline_medium_page_header_config_aligned_allocator(
	size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config)
{
	pas_aligned_allocation_result result;
	PAS_UNUSED_PARAM(size);
	PAS_UNUSED_PARAM(alignment);
	PAS_UNUSED_PARAM(large_heap);
	PAS_UNUSED_PARAM(config);
	pas_zero_memory(&result, sizeof(result));
	return result;
}

bool inline_medium_page_header_config_for_each_shared_page_directory(
	pas_segregated_heap* heap, bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg), void* arg)
{
	PAS_UNUSED_PARAM(heap);
	PAS_UNUSED_PARAM(callback);
	PAS_UNUSED_PARAM(arg);
	return true;
}

bool inline_medium_page_header_config_for_each_shared_page_directory_remote(
	pas_enumerator* enumerator, pas_segregated_heap* heap,
	bool (*callback)(pas_enumerator* enumerator, pas_segregated_shared_page_directory* directory, void* arg), void* arg)
{
	PAS_UNUSED_PARAM(enumerator);
	PAS_UNUSED_PARAM(heap);
	PAS_UNUSED_PARAM(callback);
	PAS_UNUSED_PARAM(arg);
	return true;
}

void inline_medium_page_header_config_dump_shared_page_directory_arg(pas_stream* stream, pas_segregated_shared_page_directory* directory)
{
	PAS_UNUSED_PARAM(stream);
	PAS_UNUSED_PARAM(directory);
	PAS_ASSERT(!"Should not be reached");
}

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(inline_medium_page_header_page_config, INLINE_MEDIUM_PAGE_HEADER_CONFIG.small_segregated_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(inline_medium_page_header_config, INLINE_MEDIUM_PAGE_HEADER_CONFIG);

#endif /* PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER */

#endif /* LIBPAS_ENABLED */

