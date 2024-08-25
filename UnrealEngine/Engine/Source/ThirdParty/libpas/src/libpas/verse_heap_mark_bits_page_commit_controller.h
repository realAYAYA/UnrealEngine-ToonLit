/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H
#define VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H

#include "pas_commit_mode.h"
#include "pas_lock.h"
#include "pas_segmented_vector.h"
#include "verse_heap_compact_mark_bits_page_commit_controller_ptr.h"
#include "ue_include/verse_heap_mark_bits_page_commit_controller_ue.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

#define VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_MAX_CLEAN_COUNT 3

struct verse_heap_mark_bits_page_commit_controller;
typedef struct verse_heap_mark_bits_page_commit_controller verse_heap_mark_bits_page_commit_controller;

struct verse_heap_mark_bits_page_commit_controller {
	uintptr_t chunk_base;
	pas_commit_mode is_committed;
};

extern PAS_API pas_lock verse_heap_mark_bits_page_commit_controller_commit_lock;
extern PAS_API bool verse_heap_mark_bits_page_commit_controller_is_locked;
extern PAS_API uintptr_t verse_heap_mark_bits_page_commit_controller_num_committed;
extern PAS_API uintptr_t verse_heap_mark_bits_page_commit_controller_num_decommitted;
extern PAS_API unsigned verse_heap_mark_bits_page_commit_controller_clean_count;

PAS_DECLARE_SEGMENTED_VECTOR(verse_heap_mark_bits_page_commit_controller_vector,
							 verse_heap_compact_mark_bits_page_commit_controller_ptr,
							 32);

extern PAS_API verse_heap_mark_bits_page_commit_controller_vector verse_heap_mark_bits_page_commit_controller_not_large_vector;

/* Creates a new commit controller for a chunk. Asserts that there definitely wasn't one already. Need to hold the heap lock to use this.
 
   The initial state is always committed. */
PAS_API verse_heap_mark_bits_page_commit_controller* verse_heap_mark_bits_page_commit_controller_create_not_large(uintptr_t chunk_base);

PAS_API void verse_heap_mark_bits_page_commit_controller_construct_large(verse_heap_mark_bits_page_commit_controller* controller, uintptr_t chunk_base);

PAS_API void verse_heap_mark_bits_page_commit_controller_destruct_large(verse_heap_mark_bits_page_commit_controller* controller);

/* Decommits mark bit pages if possible. It's impossible to decommit them if the GC is running. Returns true if decommit
   happened. */
PAS_API bool verse_heap_mark_bits_page_commit_controller_decommit_if_possible(void);

/* To be called periodically from the scavenger. */
PAS_API bool verse_heap_mark_bits_page_commit_controller_scavenge_periodic(void);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H */

