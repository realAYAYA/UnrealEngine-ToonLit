/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_chunk_map_entry.h"

#include "verse_heap_large_entry.h"
#include "pas_stream.h"

#if PAS_ENABLE_VERSE

void verse_heap_chunk_map_entry_dump(verse_heap_chunk_map_entry entry, pas_stream* stream)
{
    if (verse_heap_chunk_map_entry_is_empty(entry)) {
        pas_stream_printf(stream, "empty");
        return;
    }

    if (verse_heap_chunk_map_entry_is_small_segregated(entry)) {
        pas_stream_printf(stream, "small:%08x", verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(entry));
        return;
    }

    if (verse_heap_chunk_map_entry_is_medium_segregated(entry)) {
        pas_stream_printf(stream, "medium:%p/%s", verse_heap_chunk_map_entry_medium_segregated_header_object(entry), pas_empty_mode_get_string(verse_heap_chunk_map_entry_medium_segregated_empty_mode(entry)));
        return;
    }

    if (verse_heap_chunk_map_entry_is_large(entry)) {
        verse_heap_large_entry* large_entry;

        large_entry = verse_heap_chunk_map_entry_large_entry(entry);

        pas_stream_printf(stream, "large:%zx-%zx,%p", large_entry->begin, large_entry->end, large_entry->heap);
        return;
    }

    PAS_ASSERT(!"Should not be reached");
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

