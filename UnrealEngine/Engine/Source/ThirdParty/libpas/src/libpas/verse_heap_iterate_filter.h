/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_ITERATE_FILTER_H
#define VERSE_HEAP_ITERATE_FILTER_H

#include "pas_utils.h"
#include "ue_include/verse_heap_iterate_filter_ue.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

static inline const char* verse_heap_iterate_filter_get_string(verse_heap_iterate_filter filter)
{
    switch (filter) {
    case verse_heap_iterate_unmarked:
        return "unmarked";
    case verse_heap_iterate_marked:
        return "marked";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_ITERATE_FILTER_H */

