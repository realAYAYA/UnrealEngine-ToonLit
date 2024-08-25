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

#include "TestHarness.h"

#if PAS_ENABLE_HOTBIT && PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES && PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES

#include "hotbit_heap.h"
#include "hotbit_heap_config.h"
#include "inline_medium_page_header_config.h"
#include "inline_medium_page_header_heap.h"
#include "inline_non_committable_granules_config.h"
#include "inline_non_committable_granules_heap.h"
#include "outline_medium_page_header_config.h"
#include "outline_medium_page_header_heap.h"
#include "outline_non_committable_granules_config.h"
#include "outline_non_committable_granules_heap.h"
#include "pas_committed_pages_vector.h"
#include "pas_get_object_kind.h"
#include "pas_page_malloc.h"
#include "pas_scavenger.h"
#include "pas_segregated_view.h"

using namespace std;

namespace {

void testPageDecommitImpl(size_t size,
						  void* (*allocate)(size_t size),
						  void (*deallocate)(void* ptr),
						  const pas_heap_config* heapConfig,
						  const pas_segregated_page_config* pageConfig,
						  function<void(void* pageBase)> checkAfterAllocate,
						  function<void(void* pageBase)> checkAfterScavenge,
						  function<void(void* pageBase)> checkAfterAllocateAgain)
{
	static constexpr bool verbose = false;
	
	void* ptr = allocate(size);
	CHECK_EQUAL(pas_get_object_kind(ptr, *heapConfig), pas_object_kind_for_segregated_variant(pageConfig->variant));

	void* pageBase = pas_page_base_boundary_for_address_and_page_config(reinterpret_cast<uintptr_t>(ptr), pageConfig->base);

	if (verbose)
		cout << "ptr = " << ptr << ", pageBase = " << pageBase << ", page_size = " << pageConfig->base.page_size << "\n";

	checkAfterAllocate(pageBase);

	pas_segregated_view view = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(ptr), heapConfig);
	CHECK(pas_segregated_view_is_owned(view));

	deallocate(ptr);

	pas_scavenger_run_synchronously_now();

	checkAfterScavenge(pageBase);
	CHECK(!pas_segregated_view_is_owned(view));

	void* ptr2 = allocate(size);
	CHECK_EQUAL(ptr2, ptr);
	checkAfterAllocateAgain(pageBase);
	CHECK(pas_segregated_view_is_owned(view));
}

void testPageDecommitImpl(size_t size,
						  void* (*allocate)(size_t size),
						  void (*deallocate)(void* ptr),
						  const pas_heap_config* heapConfig,
						  const pas_segregated_page_config* pageConfig)
{
	testPageDecommitImpl(
		size, allocate, deallocate, heapConfig, pageConfig,
		[&] (void* pageBase) {
			size_t numCommittedPages = pas_count_committed_pages(pageBase, pageConfig->base.page_size, &allocationConfig);
			CHECK_GREATER_EQUAL(numCommittedPages, 1);
			CHECK_LESS_EQUAL(numCommittedPages, pageConfig->base.page_size >> pas_page_malloc_alignment_shift());
		},
		[&] (void* pageBase) {
			size_t numCommittedPages = pas_count_committed_pages(pageBase, pageConfig->base.page_size, &allocationConfig);
			CHECK_EQUAL(numCommittedPages, 0);
		},
		[&] (void* pageBase) {
			size_t numCommittedPages = pas_count_committed_pages(pageBase, pageConfig->base.page_size, &allocationConfig);
			CHECK_GREATER_EQUAL(numCommittedPages, 1);
			CHECK_LESS_EQUAL(numCommittedPages, pageConfig->base.page_size >> pas_page_malloc_alignment_shift());
		});
}

void testHotbitMediumPageDecommit()
{
	testPageDecommitImpl(10000, hotbit_try_allocate, hotbit_deallocate, &hotbit_heap_config, &hotbit_heap_config.medium_segregated_config);
}

void testInlineMediumPageHeaderPageDecommit()
{
	testPageDecommitImpl(10000, inline_medium_page_header_allocate, inline_medium_page_header_deallocate, &inline_medium_page_header_config, &inline_medium_page_header_config.small_segregated_config);
}

void testOutlineMediumPageHeaderPageDecommit()
{
	testPageDecommitImpl(10000, outline_medium_page_header_allocate, outline_medium_page_header_deallocate, &outline_medium_page_header_config, &outline_medium_page_header_config.small_segregated_config);
}

void testInlineNonCommittableGranulesPageDecommit()
{
	testPageDecommitImpl(
		10000, inline_non_committable_granules_allocate, inline_non_committable_granules_deallocate, &inline_non_committable_granules_config, &inline_non_committable_granules_config.small_segregated_config,
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 3, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 3, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
		});
}

void testInlineNonCommittableGranulesPageDecommitStartingDecommitted()
{
	testPageDecommitImpl(
		10000, inline_non_committable_granules_allocate, inline_non_committable_granules_deallocate, &inline_non_committable_granules_config, &inline_non_committable_granules_config.small_segregated_config,
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			pas_page_malloc_decommit(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, pas_may_mmap);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 3, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 3, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, INLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
		});
}

void testOutlineNonCommittableGranulesPageDecommit()
{
	testPageDecommitImpl(
		10000, outline_non_committable_granules_allocate, outline_non_committable_granules_deallocate, &outline_non_committable_granules_config, &outline_non_committable_granules_config.small_segregated_config,
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 6, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 2, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
		});
}

void testOutlineNonCommittableGranulesPageDecommitStartingDecommitted()
{
	testPageDecommitImpl(
		10000, outline_non_committable_granules_allocate, outline_non_committable_granules_deallocate, &outline_non_committable_granules_config, &outline_non_committable_granules_config.small_segregated_config,
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE >> pas_page_malloc_alignment_shift());
			pas_page_malloc_decommit(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, pas_may_mmap);
			pas_page_malloc_decommit(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, pas_may_mmap);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 4, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 6, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 2, &allocationConfig), 0);
		},
		[&] (void* pageBaseVoid) {
			char* pageBase = static_cast<char*>(pageBaseVoid);
			CHECK_EQUAL(pas_count_committed_pages(pageBase, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
			CHECK_EQUAL(pas_count_committed_pages(pageBase + OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE * 5, OUTLINE_NON_COMMITTABLE_GRANULES_GRANULE_SIZE, &allocationConfig), 0);
		});
}

} // anonymous namespace

#endif // PAS_ENABLE_HOTBIT && PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES && PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES

void addOddMediumPageHeaderTests()
{
#if PAS_ENABLE_HOTBIT && PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES && PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES
	ADD_TEST(testHotbitMediumPageDecommit());
	ADD_TEST(testInlineMediumPageHeaderPageDecommit());
	ADD_TEST(testOutlineMediumPageHeaderPageDecommit());
	ADD_TEST(testInlineNonCommittableGranulesPageDecommit());
	ADD_TEST(testInlineNonCommittableGranulesPageDecommitStartingDecommitted());
	ADD_TEST(testOutlineNonCommittableGranulesPageDecommit());
	ADD_TEST(testOutlineNonCommittableGranulesPageDecommitStartingDecommitted());
#endif // PAS_ENABLE_HOTBIT && PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER && PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES && PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES
}
