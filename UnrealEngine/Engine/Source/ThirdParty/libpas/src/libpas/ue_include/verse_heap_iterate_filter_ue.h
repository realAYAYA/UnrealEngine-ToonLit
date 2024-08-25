/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_ITERATE_FILTER_UE_H
#define VERSE_HEAP_ITERATE_FILTER_UE_H

enum verse_heap_iterate_filter {
    /* Iterate unmarked objects.

       Sample uses:
       - Destructing dead objects before sweep.
       - Executing fixpoint constraints on objects that can self-mark during the mark phase. */
    verse_heap_iterate_unmarked,

    /* Iterate marked objects.

       Sample uses:
       - Performing a census post-mark but pre-sweep, for example to null weak references.
       - Executing fixpoint constraints on objects whose constraints become live when those objects are
         live. */
    verse_heap_iterate_marked,
};

typedef enum verse_heap_iterate_filter verse_heap_iterate_filter;

#endif /* VERSE_HEAP_ITERATE_FILTER_UE_H */

