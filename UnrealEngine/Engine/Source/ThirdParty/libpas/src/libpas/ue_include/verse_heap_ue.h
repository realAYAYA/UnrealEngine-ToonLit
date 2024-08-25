/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_UE_H
#define VERSE_HEAP_UE_H

#include "pas_thread_local_cache_node_ue.h"
#include "verse_heap_config_ue.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pas_heap pas_heap;
typedef struct verse_heap_object_set verse_heap_object_set;
typedef struct verse_heap_page_header verse_heap_page_header;

PAS_API extern verse_heap_object_set verse_heap_all_objects;

/* This is meant to be queried directly by the Verse VM. */
PAS_API extern size_t verse_heap_live_bytes;
PAS_API extern size_t verse_heap_swept_bytes; /* Num bytes swept by the last sweep. */

/* This is meant to be set directly by the Verse VM. Anytime live bytes is found to be greater than or equal
   the threshold, the trigger callback is called. It's expected that the callback will do its own locking
   and it will use that lock to protect its changes to the threshold. */
PAS_API extern size_t verse_heap_live_bytes_trigger_threshold;
PAS_API extern void (*verse_heap_live_bytes_trigger_callback)(void);

/* Creates a heap with the requested minalign and optionally size and alignment.

   Heaps cannot be destroyed.

   If the min_align argument is smaller than VERSE_HEAP_MIN_ALIGN then the behavior is as if it was
   VERSE_HEAP_MIN_ALIGN, but it must be at least 1.

   If the size is 0, then:
   - Alignment is ignored.
   - This heap has no special virtual memory reservation; it just uses the global page cache.

   If the size is not zero, then:
   - Size must be at least VERSE_HEAP_CHUNK_SIZE.
   - Alignment must be a power of 2 and at least 1.
   - Size must be aligned to alignment.
   - The heap uses a virtual memory reservation of that size and alignment. */
PAS_API pas_heap* verse_heap_create(size_t min_align, size_t size, size_t alignment);

PAS_API void verse_heap_did_become_ready_for_allocation(void);

PAS_API void* verse_heap_get_base(pas_heap* heap);
PAS_API void verse_heap_add_to_set(pas_heap* heap, verse_heap_object_set* set);

/* This is meant to be called as the casual case of the Verse VM allocator. It can handle any size. Note that
   this is a different algorithm from pas_try_allocate_common.

   For fast case allocation, the Verse VM should maintain its own table of local allocators for the sizes and
   heaps it wants to use. */
PAS_API void* verse_heap_try_allocate(pas_heap* heap, size_t size);
PAS_API void* verse_heap_allocate(pas_heap* heap, size_t size);

/* This is mean to be called as the fast case of aligned allocation. */
PAS_API void* verse_heap_try_allocate_with_alignment(pas_heap* heap, size_t size, size_t alignment);
PAS_API void* verse_heap_allocate_with_alignment(pas_heap* heap, size_t size, size_t alignment);

/* Call this from one thread to start sweep and then do a handshake and then call the second function. */
PAS_API void verse_heap_start_allocating_black_before_handshake(void);
PAS_API void verse_heap_start_sweep_before_handshake(void);
PAS_API size_t verse_heap_start_sweep_after_handshake(void);
/* Use this to sweep in parallel (each thread calls this with a different range). */
PAS_API void verse_heap_sweep_range(size_t begin, size_t end);
/* Call this from one thread to end the sweep. */
PAS_API void verse_heap_end_sweep(void);

PAS_API pas_thread_local_cache_node* verse_heap_get_thread_local_cache_node(void);
PAS_API void verse_heap_thread_local_cache_node_stop_local_allocators(pas_thread_local_cache_node* node,
																	  uint64_t expected_version);

PAS_API uintptr_t verse_heap_find_allocated_object_start(uintptr_t inner_ptr);
PAS_API size_t verse_heap_get_allocation_size(uintptr_t inner_ptr);

PAS_API bool verse_heap_owns_address(uintptr_t ptr);

PAS_API verse_heap_page_header* verse_heap_get_page_header(uintptr_t inner_ptr);
PAS_API pas_heap* verse_heap_get_heap(uintptr_t inner_ptr);

#ifdef __cplusplus
}
#endif

#endif /* VERSE_HEAP_UE_H */
