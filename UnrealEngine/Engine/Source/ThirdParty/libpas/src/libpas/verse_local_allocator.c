/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utils.h"

#include "ue_include/verse_local_allocator_ue.h"

#include "pas_heap.h"
#include "pas_local_allocator_inlines.h"
#include "pas_segregated_heap_inlines.h"
#include "verse_heap.h"

#if PAS_ENABLE_VERSE

void verse_local_allocator_construct(pas_local_allocator* allocator, pas_heap* heap, size_t object_size, size_t allocator_size)
{
    pas_segregated_size_directory* directory;

    directory = pas_segregated_heap_size_directory_for_size(&heap->segregated_heap, object_size, VERSE_HEAP_CONFIG, NULL);

    if (!directory) {
        pas_heap_lock_lock();
        directory = pas_segregated_heap_ensure_size_directory_for_size(
            &heap->segregated_heap, object_size, 1, pas_force_size_lookup, &verse_heap_config, NULL, pas_segregated_size_directory_full_creation_mode);
        PAS_ASSERT(directory);
        pas_heap_lock_unlock();
    }

    PAS_ASSERT(directory);
    PAS_ASSERT(pas_segregated_size_directory_local_allocator_size(directory) <= allocator_size);

    pas_local_allocator_construct(allocator, directory, pas_local_allocator_not_in_thread_local_cache);
}

void verse_local_allocator_stop(pas_local_allocator* allocator)
{
    pas_local_allocator_stop(allocator, pas_lock_lock_mode_lock);
}

PAS_NEVER_INLINE static void* allocate_slow(pas_local_allocator* allocator)
{
    return (void*)pas_local_allocator_try_allocate(
        allocator, allocator->object_size, 1, VERSE_HEAP_CONFIG, &verse_heap_allocator_counts, pas_allocation_result_crash_on_error).begin;
}

void* verse_local_allocator_allocate(pas_local_allocator* allocator)
{
    pas_allocation_result result;
    void* ptr;

    /* Major care is taken here to make it so that this function has a minimal prologue and no callee-saves. That's why the slow path call is in tail position.
       That's super important! It makes this function quite fast even if it isn't inlined. */
    result = pas_local_allocator_try_allocate_inline_cases(allocator, VERSE_HEAP_CONFIG);
    if (result.did_succeed)
        ptr = (void*)result.begin;
    else
        ptr = allocate_slow(allocator);

    return ptr;
}

PAS_NEVER_INLINE static void* try_allocate_slow(pas_local_allocator* allocator)
{
    return (void*)pas_local_allocator_try_allocate(
        allocator, allocator->object_size, 1, VERSE_HEAP_CONFIG, &verse_heap_allocator_counts, pas_allocation_result_identity).begin;
}

void* verse_local_allocator_try_allocate(pas_local_allocator* allocator)
{
    pas_allocation_result result;

    result = pas_local_allocator_try_allocate_inline_cases(allocator, VERSE_HEAP_CONFIG);
    if (result.did_succeed)
        return (void*)result.begin;

    return try_allocate_slow(allocator);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

