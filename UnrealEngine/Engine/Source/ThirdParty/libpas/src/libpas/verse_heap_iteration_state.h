/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_ITERATION_STATE_H
#define VERSE_HEAP_ITERATION_STATE_H

#include "pas_utils.h"
#include "verse_heap_iterate_filter.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct verse_heap_iteration_state;
struct verse_heap_object_set;
typedef struct verse_heap_iteration_state verse_heap_iteration_state;
typedef struct verse_heap_object_set verse_heap_object_set;

struct verse_heap_iteration_state {
    uint64_t version; /* If this is 0 then we are not iterating. */
    verse_heap_object_set* set_being_iterated;
};

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_ITERATION_STATE_H */

