/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_heap_lock.h"
#include "pas_large_utility_free_heap.h"
#include "verse_heap_object_set.h"
#include "verse_heap_object_set_set.h"

#if PAS_ENABLE_VERSE

void verse_heap_object_set_set_construct(verse_heap_object_set_set* set_set)
{
    pas_heap_lock_assert_held();
    *set_set = VERSE_HEAP_OBJECT_SET_SET_INITIALIZER;
}

void verse_heap_object_set_set_add_set(verse_heap_object_set_set* set_set, verse_heap_object_set* set)
{
    pas_heap_lock_assert_held();

    PAS_ASSERT(!verse_heap_object_set_set_contains_set(set_set, set));
    
    if (set_set->num_sets >= set_set->sets_capacity) {
        unsigned new_sets_capacity;
        verse_heap_object_set** new_sets;
        
        PAS_ASSERT(set_set->num_sets == set_set->sets_capacity);

        new_sets_capacity = pas_max_uint32(4, set_set->num_sets * 2);
        new_sets = (verse_heap_object_set**)pas_large_utility_free_heap_allocate(
            new_sets_capacity * sizeof(verse_heap_object_set*),
            "verse_heap_object_set_set/sets");

        if (set_set->num_sets) {
            memcpy(new_sets, set_set->sets, set_set->num_sets * sizeof(verse_heap_object_set*));
        }

        pas_large_utility_free_heap_deallocate(set_set->sets, set_set->sets_capacity * sizeof(verse_heap_object_set*));

        set_set->sets = new_sets;
        set_set->sets_capacity = new_sets_capacity;
    }

    set_set->sets[set_set->num_sets++] = set;
}

void verse_heap_object_set_set_add_view(verse_heap_object_set_set* set_set, pas_segregated_exclusive_view* view)
{
    unsigned index;
    pas_heap_lock_assert_held();
    for (index = set_set->num_sets; index--;)
        verse_heap_object_set_add_view(set_set->sets[index], view);
}

void verse_heap_object_set_set_add_large_entry(verse_heap_object_set_set* set_set, verse_heap_large_entry* large_entry)
{
    unsigned index;
    pas_heap_lock_assert_held();
    for (index = set_set->num_sets; index--;)
        verse_heap_object_set_add_large_entry(set_set->sets[index], large_entry);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

