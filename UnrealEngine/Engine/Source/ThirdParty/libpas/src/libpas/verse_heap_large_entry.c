/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utility_heap.h"
#include "verse_heap_config.h"
#include "verse_heap_large_entry.h"
#include "verse_heap_mark_bits_page_commit_controller.h"

#if PAS_ENABLE_VERSE

verse_heap_large_entry* verse_heap_large_entry_create(uintptr_t begin, uintptr_t end, pas_heap* heap)
{
    verse_heap_large_entry* result;

    result = (verse_heap_large_entry*)pas_utility_heap_allocate(
        sizeof(verse_heap_large_entry), "verse_heap_large_entry");

    result->begin = begin;
    result->end = end;
    result->heap = heap;

	verse_heap_mark_bits_page_commit_controller_construct_large(
		&result->mark_bits_page_commit_controller, pas_round_down_to_power_of_2(begin, VERSE_HEAP_CHUNK_SIZE));

    return result;
}

void verse_heap_large_entry_destroy(verse_heap_large_entry* entry)
{
	verse_heap_mark_bits_page_commit_controller_destruct_large(&entry->mark_bits_page_commit_controller);
    pas_utility_heap_deallocate(entry);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */
