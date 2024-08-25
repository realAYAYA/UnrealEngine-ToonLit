/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_CHUNK_MAP_H
#define VERSE_HEAP_CHUNK_MAP_H

#include "pas_utils.h"
#include "ue_include/verse_heap_config_ue.h"
#include "verse_heap_chunk_map_entry.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

#define VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_BITS ((PAS_ADDRESS_BITS - VERSE_HEAP_CHUNK_SIZE_SHIFT) >> 1u)
#define VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_MASK (((uintptr_t)1 << VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_BITS) - 1)
#define VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SIZE (1u << VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_BITS)
#define VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_BITS ((PAS_ADDRESS_BITS - VERSE_HEAP_CHUNK_SIZE_SHIFT + 1) >> 1u)
#define VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_MASK (((uintptr_t)1 << VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_BITS) - 1)
#define VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SIZE (1u << VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_BITS)

#define VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SHIFT VERSE_HEAP_CHUNK_SIZE_SHIFT
#define VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SHIFT \
    (VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SHIFT + VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_BITS)

PAS_API extern verse_heap_chunk_map_entry* verse_heap_first_level_chunk_map[
    VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SIZE];

/* Check the chunk map entry for a chunk; if we know nothing about a chunk then we will return an empty
   chunk map entry. */
static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry verse_heap_get_chunk_map_entry(uintptr_t address)
{
    verse_heap_chunk_map_entry* second_level;
    verse_heap_chunk_map_entry result;

    if (address > PAS_MAX_ADDRESS)
        return verse_heap_chunk_map_entry_create_empty();

    /* FIXME: Do we need the mask here? */
    second_level = verse_heap_first_level_chunk_map[
        (address >> VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SHIFT) & VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_MASK];
    if (!second_level)
        return verse_heap_chunk_map_entry_create_empty();

    verse_heap_chunk_map_entry_copy_atomically(
        &result,
        second_level + ((address >> VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SHIFT)
                        & VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_MASK));
    return result;
}

/* Get a pointer to a chunk map entry. This assumes that the chunk map entry must exist. It may crash or
   do weird stuff if it doesn't. */
static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry* verse_heap_get_chunk_map_entry_ptr(uintptr_t address)
{
    verse_heap_chunk_map_entry* second_level;

    PAS_ASSERT(address <= PAS_MAX_ADDRESS);

    second_level = verse_heap_first_level_chunk_map[
        (address >> VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SHIFT) & VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_MASK];
    PAS_TESTING_ASSERT(second_level);

    return second_level + ((address >> VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SHIFT)
                           & VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_MASK);
}

/* This may allocate a second-level chunk map on-demand if needed. Requires holding the heap lock. May
   return an existing chunk map entry if one did not exist before. If one did not exist before then the
   entry will already be zero. Hence a valid idiom for using this is to ignore the return value. */
PAS_API verse_heap_chunk_map_entry* verse_heap_initialize_chunk_map_entry_ptr(uintptr_t address);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_CHUNK_MAP_H */

