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

#ifndef INLINE_NON_COMMITTABLE_GRANULES_CONFIG_H
#define INLINE_NON_COMMITTABLE_GRANULES_CONFIG_H

#include "pas_config.h"

#if PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES

#include "pas_heap_config_utils.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

/* This config and heap exist to test having noncommittable granules combined with an inline header. */

#define INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN_SHIFT 10u
#define INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN (1u << INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN_SHIFT)
#define INLINE_NON_COMMITTABLE_GRANULES_PAGE_SIZE 131072
#define INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE 16384
#define INLINE_NON_COMMITTABLE_GRANULES_HEADER_SIZE PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE( \
    INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN_SHIFT, INLINE_NON_COMMITTABLE_GRANULES_PAGE_SIZE, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE)
#define INLINE_NON_COMMITTABLE_GRANULES_PAYLOAD_SIZE (INLINE_NON_COMMITTABLE_GRANULES_PAGE_SIZE - INLINE_NON_COMMITTABLE_GRANULES_HEADER_SIZE)

PAS_API pas_page_base* inline_non_committable_granules_config_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary);

static inline pas_page_base* inline_non_committable_granules_config_header_for_boundary(void* boundary)
{
	return (pas_page_base*)boundary;
}
static inline void* inline_non_committable_granules_config_boundary_for_header(pas_page_base* page)
{
	return page;
}

PAS_API void* inline_non_committable_granules_config_allocate_page(pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
PAS_API pas_page_base* inline_non_committable_granules_config_create_page_header(void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void inline_non_committable_granules_config_destroy_page_header(pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API pas_segregated_shared_page_directory* inline_non_committable_granules_config_shared_page_directory_selector(
	pas_segregated_heap* heap, pas_segregated_size_directory* directory);

static inline pas_fast_megapage_kind inline_non_committable_granules_config_fast_megapage_kind(uintptr_t begin)
{
	PAS_UNUSED_PARAM(begin);
	return pas_small_exclusive_segregated_fast_megapage_kind;
}

static inline pas_page_base* inline_non_committable_granules_config_page_header(uintptr_t begin)
{
	PAS_UNUSED_PARAM(begin);
	return NULL;
}

PAS_API pas_aligned_allocation_result inline_non_committable_granules_config_aligned_allocator(
	size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config);
PAS_API bool inline_non_committable_granules_config_for_each_shared_page_directory(
	pas_segregated_heap* heap, bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg), void* arg);
PAS_API bool inline_non_committable_granules_config_for_each_shared_page_directory_remote(
	pas_enumerator* enumerator, pas_segregated_heap* heap,
	bool (*callback)(pas_enumerator* enumerator, pas_segregated_shared_page_directory* directory, void* arg), void* arg);
PAS_API void inline_non_committable_granules_config_dump_shared_page_directory_arg(pas_stream* stream, pas_segregated_shared_page_directory* directory);

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(inline_non_committable_granules_page_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(inline_non_committable_granules_config);

PAS_API extern const unsigned inline_non_committable_granules_config_non_committable_granule_bitvector[];

#define INLINE_NON_COMMITTABLE_GRANULES_CONFIG ((pas_heap_config){ \
        .config_ptr = &inline_non_committable_granules_config, \
		.kind = pas_heap_config_kind_inline_non_committable_granules, \
		.activate_callback = pas_heap_config_utils_null_activate, \
		.get_type_size = pas_simple_type_as_heap_type_get_type_size, \
		.get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
		.dump_type = pas_simple_type_as_heap_type_dump, \
		.large_alignment = INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN, \
		.small_segregated_config = { \
		    .base = { \
			    .is_enabled = true, \
				.heap_config_ptr = &inline_non_committable_granules_config, \
				.page_config_ptr = &inline_non_committable_granules_config.small_segregated_config.base, \
				.page_config_kind = pas_page_config_kind_segregated, \
				.min_align_shift = INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN_SHIFT, \
				.page_size = INLINE_NON_COMMITTABLE_GRANULES_PAGE_SIZE, \
				.granule_size = INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, \
			    .non_committable_granule_bitvector = inline_non_committable_granules_config_non_committable_granule_bitvector, \
				.max_object_size = 20000, \
				.page_header_for_boundary = inline_non_committable_granules_config_header_for_boundary, \
				.boundary_for_page_header = inline_non_committable_granules_config_boundary_for_header, \
				.page_header_for_boundary_remote = inline_non_committable_granules_config_header_for_boundary_remote, \
				.create_page_header = inline_non_committable_granules_config_create_page_header, \
				.destroy_page_header = inline_non_committable_granules_config_destroy_page_header \
			}, \
			.variant = pas_small_segregated_page_config_variant, \
			.kind = pas_segregated_page_config_kind_inline_non_committable_granules, \
			.wasteage_handicap = 1., \
			.sharing_shift = 10, \
			.num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(INLINE_NON_COMMITTABLE_GRANULES_MIN_ALIGN_SHIFT, INLINE_NON_COMMITTABLE_GRANULES_PAGE_SIZE), \
			.shared_payload_offset = 0, \
			.exclusive_payload_offset = INLINE_NON_COMMITTABLE_GRANULES_HEADER_SIZE, \
			.shared_payload_size = 0, \
			.exclusive_payload_size = INLINE_NON_COMMITTABLE_GRANULES_PAYLOAD_SIZE, \
			.shared_logging_mode = pas_segregated_deallocation_no_logging_mode, \
			.exclusive_logging_mode = pas_segregated_deallocation_no_logging_mode, \
			.use_reversed_current_word = false, \
			.check_deallocation = false, \
			.enable_empty_word_eligibility_optimization_for_shared = false, \
			.enable_empty_word_eligibility_optimization_for_exclusive = false, \
			.enable_view_cache = false, \
			.page_allocator = inline_non_committable_granules_config_allocate_page, \
			.shared_page_directory_selector = inline_non_committable_granules_config_shared_page_directory_selector, \
			PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(inline_non_committable_granules_page_config) \
		}, \
		.medium_segregated_config = { \
	        .base = { \
			    .is_enabled = false \
			} \
		}, \
		.small_bitfit_config = { \
		    .base = { \
			    .is_enabled = false \
			} \
		}, \
		.medium_bitfit_config = { \
		    .base = { \
			    .is_enabled = false \
			} \
		}, \
		.marge_bitfit_config = { \
		    .base = { \
			    .is_enabled = false \
			} \
		}, \
		.small_lookup_size_upper_bound = PAS_SMALL_LOOKUP_SIZE_UPPER_BOUND, \
		.fast_megapage_kind_func = inline_non_committable_granules_config_fast_megapage_kind, \
		.small_segregated_is_in_megapage = true, \
		.small_bitfit_is_in_megapage = false, \
		.page_header_func = inline_non_committable_granules_config_page_header, \
		.aligned_allocator = inline_non_committable_granules_config_aligned_allocator, \
		.aligned_allocator_talks_to_sharing_pool = false, \
		.deallocator = NULL, \
		.mmap_capability = pas_may_mmap, \
		.root_data = NULL, \
		.prepare_to_enumerate = NULL, \
		.for_each_shared_page_directory = inline_non_committable_granules_config_for_each_shared_page_directory, \
		.for_each_shared_page_directory_remote = inline_non_committable_granules_config_for_each_shared_page_directory_remote, \
		.dump_shared_page_directory_arg = inline_non_committable_granules_config_dump_shared_page_directory_arg, \
		PAS_HEAP_CONFIG_SPECIALIZATIONS(inline_non_committable_granules_config) \
	})

PAS_API extern const pas_heap_config inline_non_committable_granules_config;

PAS_API extern pas_heap_runtime_config inline_non_committable_granules_runtime_config;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES */

#endif /* INLINE_NON_COMMITTABLE_GRANULES_CONFIG_H */

