/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_OBJECT_SET_H
#define VERSE_HEAP_OBJECT_SET_H

#include "pas_compact_atomic_segregated_exclusive_view_ptr.h"
#include "pas_segmented_vector.h"
#include "verse_heap_compact_large_entry_ptr.h"
#include "verse_heap_iterate_filter.h"
#include "ue_include/verse_heap_object_set_ue.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct verse_heap_object_set;
typedef struct verse_heap_object_set verse_heap_object_set;

PAS_DECLARE_SEGMENTED_VECTOR(verse_heap_view_vector,
                             pas_compact_atomic_segregated_exclusive_view_ptr,
                             128);

struct verse_heap_object_set {
    verse_heap_compact_large_entry_ptr* large_entries;
    size_t num_large_entries;
    size_t large_entries_capacity;

    verse_heap_view_vector views;
};

#define VERSE_HEAP_OBJECT_SET_INITIALIZER ((verse_heap_object_set){ \
        .large_entries = NULL, \
        .num_large_entries = 0, \
        .large_entries_capacity = 0, \
        .views = PAS_SEGMENTED_VECTOR_INITIALIZER, \
    })

PAS_API void verse_heap_object_set_add_view(verse_heap_object_set* set, pas_segregated_exclusive_view* view);
PAS_API void verse_heap_object_set_add_large_entry(verse_heap_object_set* set, verse_heap_large_entry* entry);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_OBJECT_SET_H */

