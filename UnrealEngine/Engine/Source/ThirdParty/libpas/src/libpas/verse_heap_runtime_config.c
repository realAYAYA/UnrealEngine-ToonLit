/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_runtime_config.h"

#include "verse_heap.h"

#if PAS_ENABLE_VERSE

pas_allocation_result verse_heap_runtime_config_allocate_chunks(verse_heap_runtime_config* config,
                                                                size_t size,
																pas_physical_memory_transaction* transaction,
																pas_primordial_page_state desired_state)
{
    pas_allocation_result result;

    PAS_ASSERT(pas_is_aligned(size, VERSE_HEAP_CHUNK_SIZE));

    result = config->page_provider(
		size, pas_alignment_create_traditional(VERSE_HEAP_CHUNK_SIZE), "verse_heap_chunk", NULL, transaction, desired_state,
		config->page_provider_arg);
    
    if (result.did_succeed) {
        uintptr_t address;
        PAS_ASSERT(result.zero_mode);
        for (address = result.begin; address < result.begin + size; address += VERSE_HEAP_CHUNK_SIZE)
            verse_heap_initialize_chunk_map_entry_ptr(address);
    }
    
    return result;
}

pas_allocation_result verse_heap_runtime_config_chunks_provider(size_t size,
																pas_alignment alignment,
																const char* name,
																pas_heap* heap,
																pas_physical_memory_transaction* transaction,
																pas_primordial_page_state desired_state,
																void* arg)
{
	verse_heap_runtime_config* config;

    PAS_UNUSED_PARAM(heap);

    PAS_ASSERT(pas_is_aligned(size, VERSE_HEAP_CHUNK_SIZE));
    PAS_ASSERT(!alignment.alignment_begin);
    PAS_ASSERT(alignment.alignment == VERSE_HEAP_CHUNK_SIZE);

	config = (verse_heap_runtime_config*)arg;

	return verse_heap_runtime_config_allocate_chunks(config, size, transaction, desired_state);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */


