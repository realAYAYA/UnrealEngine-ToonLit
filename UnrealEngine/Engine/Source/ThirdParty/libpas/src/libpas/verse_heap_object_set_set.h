/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_OBJECT_SET_SET_H
#define VERSE_HEAP_OBJECT_SET_SET_H

#include "pas_segregated_view.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct verse_heap_large_entry;
struct verse_heap_object_set;
struct verse_heap_object_set_set;
typedef struct verse_heap_large_entry verse_heap_large_entry;
typedef struct verse_heap_object_set verse_heap_object_set;
typedef struct verse_heap_object_set_set verse_heap_object_set_set;

/* This is the set of sets that a heap belongs to. */
struct verse_heap_object_set_set {
    /* FIXME: This should almost certainly be a fixed-size array to reduce pointer chasing, since no set of
       object sets will be that big. Maybe they're bounded at like 2 or 3 in practice. */
    verse_heap_object_set** sets;
    unsigned num_sets;
    unsigned sets_capacity;
};

#define VERSE_HEAP_OBJECT_SET_SET_INITIALIZER ((verse_heap_object_set_set){ \
        .sets = NULL, \
        .num_sets = 0, \
        .sets_capacity = 0 \
    })

PAS_API void verse_heap_object_set_set_construct(verse_heap_object_set_set* set_set);

/* This should be called before any objects are allocated. */
PAS_API void verse_heap_object_set_set_add_set(verse_heap_object_set_set* set_set, verse_heap_object_set* set);

static PAS_ALWAYS_INLINE bool verse_heap_object_set_set_contains_set(verse_heap_object_set_set* set_set,
                                                                     verse_heap_object_set* set)
{
    unsigned index;

    for (index = set_set->num_sets; index--;) {
        if (set_set->sets[index] == set)
            return true;
    }

    return false;
}

PAS_API void verse_heap_object_set_set_add_view(verse_heap_object_set_set* set_set,
                                                pas_segregated_exclusive_view* view);
PAS_API void verse_heap_object_set_set_add_large_entry(verse_heap_object_set_set* set_set,
                                                       verse_heap_large_entry* entry);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_OBJECT_SET_SET_H */


