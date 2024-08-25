/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_mark_bits_page_commit_controller.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_scavenger.h"
#include "verse_heap.h"
#include "verse_heap_large_entry.h"

#if PAS_ENABLE_VERSE

pas_lock verse_heap_mark_bits_page_commit_controller_commit_lock = PAS_LOCK_INITIALIZER;
bool verse_heap_mark_bits_page_commit_controller_is_locked = false;
uintptr_t verse_heap_mark_bits_page_commit_controller_num_committed = 0;
uintptr_t verse_heap_mark_bits_page_commit_controller_num_decommitted = 0;
unsigned verse_heap_mark_bits_page_commit_controller_clean_count = 0;
verse_heap_mark_bits_page_commit_controller_vector verse_heap_mark_bits_page_commit_controller_not_large_vector = PAS_SEGMENTED_VECTOR_INITIALIZER;

static void construct(verse_heap_mark_bits_page_commit_controller* controller, uintptr_t chunk_base)
{
	verse_heap_mark_bits_page_commit_controller_clean_count = 0;
	pas_scavenger_did_create_eligible();
	controller->chunk_base = chunk_base;
	controller->is_committed = pas_committed;
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_committed, 1);
}

verse_heap_mark_bits_page_commit_controller* verse_heap_mark_bits_page_commit_controller_create_not_large(uintptr_t chunk_base)
{
	verse_heap_mark_bits_page_commit_controller* result;
	verse_heap_compact_mark_bits_page_commit_controller_ptr ptr;
	
	pas_heap_lock_assert_held();
	PAS_ASSERT(pas_is_aligned(chunk_base, VERSE_HEAP_CHUNK_SIZE));

	result = pas_immortal_heap_allocate(sizeof(verse_heap_mark_bits_page_commit_controller), "verse_heap_mark_bits_page_commit_controller", pas_object_allocation);

	construct(result, chunk_base);

	verse_heap_compact_mark_bits_page_commit_controller_ptr_store(&ptr, result);
	verse_heap_mark_bits_page_commit_controller_vector_append(&verse_heap_mark_bits_page_commit_controller_not_large_vector, ptr);

	return result;
}

void verse_heap_mark_bits_page_commit_controller_construct_large(verse_heap_mark_bits_page_commit_controller* controller, uintptr_t chunk_base)
{
	construct(controller, chunk_base);
}

void verse_heap_mark_bits_page_commit_controller_destruct_large(verse_heap_mark_bits_page_commit_controller* controller)
{
	PAS_ASSERT(controller->is_committed);
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_committed, -1);
}

static bool for_each_mark_bits_page_commit_controller_vector_callback(verse_heap_compact_mark_bits_page_commit_controller_ptr* ptr, size_t index, void *arg)
{
	verse_heap_mark_bits_page_commit_controller* controller;
	void (*callback)(verse_heap_mark_bits_page_commit_controller* controller);
	
	PAS_UNUSED_PARAM(index);

	controller = verse_heap_compact_mark_bits_page_commit_controller_ptr_load_non_null(ptr);
	callback = (void (*)(verse_heap_mark_bits_page_commit_controller* controller))arg;

	callback(controller);

	return true;
}

static void for_each_mark_bits_page_commit_controller(void (*callback)(verse_heap_mark_bits_page_commit_controller* controller))
{
	size_t index;
	
	verse_heap_mark_bits_page_commit_controller_vector_iterate(
		&verse_heap_mark_bits_page_commit_controller_not_large_vector,
		0, for_each_mark_bits_page_commit_controller_vector_callback, callback);

	pas_heap_lock_lock();
	for (index = 0; index < verse_heap_all_objects.num_large_entries; ++index) {
		verse_heap_large_entry* entry;

		entry = verse_heap_compact_large_entry_ptr_load_non_null(verse_heap_all_objects.large_entries + index);

		pas_heap_lock_unlock();
		callback(&entry->mark_bits_page_commit_controller);
		pas_heap_lock_lock();
	}
	pas_heap_lock_unlock();
}

static void lock_callback(verse_heap_mark_bits_page_commit_controller* controller)
{
	pas_lock_assert_held(&verse_heap_mark_bits_page_commit_controller_commit_lock);

	if (controller->is_committed)
		return;

	pas_page_malloc_commit((void*)controller->chunk_base, VERSE_HEAP_PAGE_SIZE, pas_may_mmap);
	PAS_ASSERT(!controller->is_committed);
	controller->is_committed = pas_committed;
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_committed, 1);
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_decommitted, -1);
}

void verse_heap_mark_bits_page_commit_controller_lock(void)
{
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_is_locked);
	pas_lock_lock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_is_locked);
	verse_heap_mark_bits_page_commit_controller_is_locked = true;
	if (verse_heap_mark_bits_page_commit_controller_num_decommitted)
		for_each_mark_bits_page_commit_controller(lock_callback);
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_num_decommitted);
	pas_lock_unlock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_num_decommitted);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
}

void verse_heap_mark_bits_page_commit_controller_unlock(void)
{
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_num_decommitted);
	pas_lock_lock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
	verse_heap_mark_bits_page_commit_controller_is_locked = false;
	verse_heap_mark_bits_page_commit_controller_clean_count = 0;
	PAS_ASSERT(!verse_heap_mark_bits_page_commit_controller_num_decommitted);
	pas_lock_unlock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	pas_scavenger_did_create_eligible();
	pas_scavenger_notify_eligibility_if_needed();
}

static void decommit_callback(verse_heap_mark_bits_page_commit_controller* controller)
{
	PAS_TESTING_ASSERT(!verse_heap_mark_bits_page_commit_controller_is_locked);
	pas_lock_testing_assert_held(&verse_heap_mark_bits_page_commit_controller_commit_lock);

	if (!controller->is_committed)
		return;

	controller->is_committed = pas_decommitted;
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_committed, -1);
	pas_atomic_exchange_add_uintptr(&verse_heap_mark_bits_page_commit_controller_num_decommitted, 1);

	pas_page_malloc_decommit((void*)controller->chunk_base, VERSE_HEAP_PAGE_SIZE, pas_may_mmap);
	PAS_ASSERT(!controller->is_committed);
}

static bool try_decommit(void)
{
	pas_lock_assert_held(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	if (verse_heap_mark_bits_page_commit_controller_is_locked)
		return false;
	if (verse_heap_mark_bits_page_commit_controller_num_committed)
		for_each_mark_bits_page_commit_controller(decommit_callback);
	return true;
}

static void decommit(void)
{
	bool result;
	result = try_decommit();
	PAS_ASSERT(result);
}

bool verse_heap_mark_bits_page_commit_controller_decommit_if_possible(void)
{
	bool result;
	pas_lock_lock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	result = try_decommit();
	pas_lock_unlock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	return result;
}

static bool scavenge_periodic_impl(void)
{
	if (verse_heap_mark_bits_page_commit_controller_is_locked)
		return false;
	if (!verse_heap_mark_bits_page_commit_controller_num_committed)
		return false;
	if (verse_heap_mark_bits_page_commit_controller_clean_count >= VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_MAX_CLEAN_COUNT) {
		decommit();
		return !!verse_heap_mark_bits_page_commit_controller_num_committed;
	}
	verse_heap_mark_bits_page_commit_controller_clean_count++;
	return true;
}

bool verse_heap_mark_bits_page_commit_controller_scavenge_periodic(void)
{
	bool result;
	pas_lock_lock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	result = scavenge_periodic_impl();
	pas_lock_unlock(&verse_heap_mark_bits_page_commit_controller_commit_lock);
	return result;
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

