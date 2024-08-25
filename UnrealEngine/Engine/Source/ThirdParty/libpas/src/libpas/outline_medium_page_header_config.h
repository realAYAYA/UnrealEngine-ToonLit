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

#ifndef OUTLINE_MEDIUM_PAGE_HEADER_CONFIG_H
#define OUTLINE_MEDIUM_PAGE_HEADER_CONFIG_H

#include "pas_config.h"

#if PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER

#include "pas_heap_config_utils.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

/* This config and heap exist to test the case where a medium page (i.e. page with more than one granule) has an out-of-line page header.
 
   This is quite redundant since medium page configs currently always have out-of-line page headers in shipping configs. But, having a config
   like this complements the inline_medium_page_header config and serves as a good template for other test configs. It's also great to know
   that a config that has outline page headers is this easy to write. */

#define OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN_SHIFT 10u
#define OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN (1u << OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN_SHIFT)
#define OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE 131072
#define OUTLINE_MEDIUM_PAGE_HEADER_GRANULE_SIZE 16384

#define OUTLINE_MEDIUM_PAGE_HEADER_HEADER_SIZE PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE( \
    OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN_SHIFT, OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, OUTLINE_MEDIUM_PAGE_HEADER_GRANULE_SIZE)

PAS_API extern pas_page_header_table outline_medium_page_header_table;

PAS_API pas_page_base* outline_medium_page_header_config_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary);

static inline pas_page_base* outline_medium_page_header_config_header_for_boundary(void* boundary)
{
	return pas_page_header_table_get_for_boundary(&outline_medium_page_header_table, OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, boundary);
}
static inline void* outline_medium_page_header_config_boundary_for_header(pas_page_base* page)
{
	return pas_page_header_table_get_boundary(&outline_medium_page_header_table, OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, page);
}

PAS_API void* outline_medium_page_header_config_allocate_page(pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
PAS_API pas_page_base* outline_medium_page_header_config_create_page_header(void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void outline_medium_page_header_config_destroy_page_header(pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API pas_segregated_shared_page_directory* outline_medium_page_header_config_shared_page_directory_selector(
	pas_segregated_heap* heap, pas_segregated_size_directory* directory);

static inline pas_fast_megapage_kind outline_medium_page_header_config_fast_megapage_kind(uintptr_t begin)
{
	PAS_UNUSED_PARAM(begin);
	return pas_not_a_fast_megapage_kind;
}

static inline pas_page_base* outline_medium_page_header_config_page_header(uintptr_t begin)
{
	return pas_page_header_table_get_for_address(&outline_medium_page_header_table, OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, (void*)begin);
}

PAS_API pas_aligned_allocation_result outline_medium_page_header_config_aligned_allocator(
	size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config);
PAS_API bool outline_medium_page_header_config_for_each_shared_page_directory(
	pas_segregated_heap* heap, bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg), void* arg);
PAS_API bool outline_medium_page_header_config_for_each_shared_page_directory_remote(
	pas_enumerator* enumerator, pas_segregated_heap* heap,
	bool (*callback)(pas_enumerator* enumerator, pas_segregated_shared_page_directory* directory, void* arg), void* arg);
PAS_API void outline_medium_page_header_config_dump_shared_page_directory_arg(pas_stream* stream, pas_segregated_shared_page_directory* directory);

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(outline_medium_page_header_page_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(outline_medium_page_header_config);

#define OUTLINE_MEDIUM_PAGE_HEADER_CONFIG ((pas_heap_config){ \
        .config_ptr = &outline_medium_page_header_config, \
		.kind = pas_heap_config_kind_outline_medium_page_header, \
		.activate_callback = pas_heap_config_utils_null_activate, \
		.get_type_size = pas_simple_type_as_heap_type_get_type_size, \
		.get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
		.dump_type = pas_simple_type_as_heap_type_dump, \
		.large_alignment = OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN, \
		.small_segregated_config = { \
		    .base = { \
			    .is_enabled = true, \
				.heap_config_ptr = &outline_medium_page_header_config, \
				.page_config_ptr = &outline_medium_page_header_config.small_segregated_config.base, \
				.page_config_kind = pas_page_config_kind_segregated, \
				.min_align_shift = OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN_SHIFT, \
				.page_size = OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, \
				.granule_size = OUTLINE_MEDIUM_PAGE_HEADER_GRANULE_SIZE, \
			    .non_committable_granule_bitvector = NULL, \
				.max_object_size = 20000, \
				.page_header_for_boundary = outline_medium_page_header_config_header_for_boundary, \
				.boundary_for_page_header = outline_medium_page_header_config_boundary_for_header, \
				.page_header_for_boundary_remote = outline_medium_page_header_config_header_for_boundary_remote, \
				.create_page_header = outline_medium_page_header_config_create_page_header, \
				.destroy_page_header = outline_medium_page_header_config_destroy_page_header \
			}, \
			.variant = pas_small_segregated_page_config_variant, \
			.kind = pas_segregated_page_config_kind_outline_medium_page_header, \
			.wasteage_handicap = 1., \
			.sharing_shift = 10, \
			.num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(OUTLINE_MEDIUM_PAGE_HEADER_MIN_ALIGN_SHIFT, OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE), \
			.shared_payload_offset = 0, \
			.exclusive_payload_offset = 0, \
			.shared_payload_size = 0, \
			.exclusive_payload_size = OUTLINE_MEDIUM_PAGE_HEADER_PAGE_SIZE, \
			.shared_logging_mode = pas_segregated_deallocation_no_logging_mode, \
			.exclusive_logging_mode = pas_segregated_deallocation_no_logging_mode, \
			.use_reversed_current_word = false, \
			.check_deallocation = false, \
			.enable_empty_word_eligibility_optimization_for_shared = false, \
			.enable_empty_word_eligibility_optimization_for_exclusive = false, \
			.enable_view_cache = false, \
			.page_allocator = outline_medium_page_header_config_allocate_page, \
			.shared_page_directory_selector = outline_medium_page_header_config_shared_page_directory_selector, \
			PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(outline_medium_page_header_page_config) \
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
		.fast_megapage_kind_func = outline_medium_page_header_config_fast_megapage_kind, \
		.small_segregated_is_in_megapage = false, \
		.small_bitfit_is_in_megapage = false, \
		.page_header_func = outline_medium_page_header_config_page_header, \
		.aligned_allocator = outline_medium_page_header_config_aligned_allocator, \
		.aligned_allocator_talks_to_sharing_pool = false, \
		.deallocator = NULL, \
		.mmap_capability = pas_may_mmap, \
		.root_data = NULL, \
		.prepare_to_enumerate = NULL, \
		.for_each_shared_page_directory = outline_medium_page_header_config_for_each_shared_page_directory, \
		.for_each_shared_page_directory_remote = outline_medium_page_header_config_for_each_shared_page_directory_remote, \
		.dump_shared_page_directory_arg = outline_medium_page_header_config_dump_shared_page_directory_arg, \
		PAS_HEAP_CONFIG_SPECIALIZATIONS(outline_medium_page_header_config) \
	})

PAS_API extern const pas_heap_config outline_medium_page_header_config;

PAS_API extern pas_heap_runtime_config outline_medium_page_header_runtime_config;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER */

#endif /* OUTLINE_MEDIUM_PAGE_HEADER_CONFIG_H */

