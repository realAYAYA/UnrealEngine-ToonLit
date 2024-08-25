/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_LOCAL_ALLOCATOR_UE_H
#define VERSE_LOCAL_ALLOCATOR_UE_H

#include "pas_local_allocator_ue.h"
#include "verse_heap_config_ue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pas_heap pas_heap;

#define VERSE_SMALL_SEGREGATED_LOCAL_ALLOCATOR_SIZE PAS_FAKE_LOCAL_ALLOCATOR_SIZE(VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE >> VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_MEDIUM_SEGREGATED_LOCAL_ALLOCATOR_SIZE PAS_FAKE_LOCAL_ALLOCATOR_SIZE(VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE >> VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_MAX_SEGREGATED_LOCAL_ALLOCATOR_SIZE ( \
    VERSE_SMALL_SEGREGATED_LOCAL_ALLOCATOR_SIZE > VERSE_MEDIUM_SEGREGATED_LOCAL_ALLOCATOR_SIZE \
    ? VERSE_SMALL_SEGREGATED_LOCAL_ALLOCATOR_SIZE : VERSE_MEDIUM_SEGREGATED_LOCAL_ALLOCATOR_SIZE)

PAS_API void verse_local_allocator_construct(pas_local_allocator* allocator, pas_heap* heap, size_t object_size, size_t allocator_size);
PAS_API void verse_local_allocator_stop(pas_local_allocator* allocator);
PAS_API void* verse_local_allocator_allocate(pas_local_allocator* allocator);
PAS_API void* verse_local_allocator_try_allocate(pas_local_allocator* allocator);
    
#ifdef __cplusplus
}
#endif

#endif /* VERSE_LOCAL_ALLOCATOR_H */

