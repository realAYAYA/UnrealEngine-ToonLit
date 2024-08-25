/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_BLACK_ALLOCATION_MODE_H
#define VERSE_HEAP_BLACK_ALLOCATION_MODE_H

#include "pas_utils.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

enum verse_heap_black_allocation_mode {
    verse_heap_do_not_allocate_black,
    verse_heap_allocate_black
};

typedef enum verse_heap_black_allocation_mode verse_heap_black_allocation_mode;

static inline const char* verse_heap_black_allocation_mode_get_string(verse_heap_black_allocation_mode mode)
{
    switch (mode) {
    case verse_heap_do_not_allocate_black:
        return "do_not_allocate_black";
    case verse_heap_allocate_black:
        return "allocate_black";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_BLACK_ALLOCATION_MODE_H */

