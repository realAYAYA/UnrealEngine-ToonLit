/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_object_set.h"

#include "pas_immortal_heap.h"
#include "pas_large_utility_free_heap.h"
#include "verse_heap.h"
#include "verse_heap_object_set_inlines.h"

#if PAS_ENABLE_VERSE

verse_heap_object_set* verse_heap_object_set_create(void)
{
    verse_heap_object_set* result;

    pas_heap_lock_lock();
    PAS_ASSERT(!verse_heap_is_ready_for_allocation);

    result = (verse_heap_object_set*)pas_immortal_heap_allocate(sizeof(verse_heap_object_set),
                                                                "verse_heap_object_set",
                                                                pas_object_allocation);
    *result = VERSE_HEAP_OBJECT_SET_INITIALIZER;

    verse_heap_object_set_set_add_set(&verse_heap_all_sets, result);

    pas_heap_lock_unlock();

    return result;
}

void verse_heap_object_set_add_view(verse_heap_object_set* set, pas_segregated_exclusive_view* view)
{
    pas_compact_atomic_segregated_exclusive_view_ptr ptr;
    pas_heap_lock_assert_held();
    pas_compact_atomic_segregated_exclusive_view_ptr_store(&ptr, view);
    verse_heap_view_vector_append(&set->views, ptr);
}

void verse_heap_object_set_add_large_entry(verse_heap_object_set* set, verse_heap_large_entry* entry)
{
    if (set->num_large_entries >= set->large_entries_capacity) {
        verse_heap_compact_large_entry_ptr* new_large_entries;
        size_t new_large_entries_capacity;
        
        PAS_ASSERT(set->num_large_entries == set->large_entries_capacity);

        new_large_entries_capacity = pas_max_uintptr(100, set->num_large_entries * 2);
        new_large_entries = (verse_heap_compact_large_entry_ptr*)pas_large_utility_free_heap_allocate(
            new_large_entries_capacity * sizeof(verse_heap_compact_large_entry_ptr),
            "verse_heap_object_set/large_entries");
        if (set->num_large_entries) {
            memcpy(new_large_entries, set->large_entries,
                   set->num_large_entries * sizeof(verse_heap_compact_large_entry_ptr));
        }

        pas_large_utility_free_heap_deallocate(
            set->large_entries, set->large_entries_capacity * sizeof(verse_heap_compact_large_entry_ptr));

        set->large_entries_capacity = new_large_entries_capacity;
        set->large_entries = new_large_entries;
    }

    verse_heap_compact_large_entry_ptr_store(set->large_entries + set->num_large_entries++, entry);
}

void verse_heap_object_set_start_iterate_before_handshake(verse_heap_object_set* set)
{
    static const bool verbose = false;
    
    PAS_ASSERT(!verse_heap_current_iteration_state.version);
    PAS_ASSERT(!verse_heap_is_sweeping);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);

	/* Copying the num_large_entries can happen before or after the store fence below; it's totally unrelated to
	   the setup of the iteration state. We just need to store the num_large_entries sometime before the
	   handshake. */
	PAS_ASSERT(verse_heap_num_large_entries_for_iteration == SIZE_MAX);
	verse_heap_num_large_entries_for_iteration = set->num_large_entries;
	
    verse_heap_current_iteration_state.set_being_iterated = set;
    pas_store_store_fence();
    verse_heap_current_iteration_state.version = ++verse_heap_latest_version;

    if (verbose)
        pas_log("Iterating with version %" PRIu64 "\n", verse_heap_current_iteration_state.version);
}

size_t verse_heap_object_set_start_iterate_after_handshake(verse_heap_object_set* set)
{
    PAS_ASSERT(verse_heap_current_iteration_state.version == verse_heap_latest_version);
    PAS_ASSERT(!verse_heap_is_sweeping);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    return 1 + set->views.size;
}

void verse_heap_object_set_iterate_range(
    verse_heap_object_set* set,
    size_t begin,
    size_t end,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void* arg)
{
	verse_heap_object_set_iterate_range_inline(set, begin, end, filter, callback, arg);
}
	
void verse_heap_object_set_end_iterate(verse_heap_object_set* set)
{
    PAS_ASSERT(verse_heap_current_iteration_state.version == verse_heap_latest_version);
    PAS_ASSERT(!verse_heap_is_sweeping);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    verse_heap_current_iteration_state.version = 0;
    pas_store_store_fence();
    verse_heap_current_iteration_state.set_being_iterated = NULL;

	PAS_ASSERT(verse_heap_num_large_entries_for_iteration <= set->num_large_entries);
	verse_heap_num_large_entries_for_iteration = SIZE_MAX;
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

