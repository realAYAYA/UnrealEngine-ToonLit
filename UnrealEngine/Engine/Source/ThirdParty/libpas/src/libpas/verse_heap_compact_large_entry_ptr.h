/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_COMPACT_LARGE_ENTRY_PTR_H
#define VERSE_HEAP_COMPACT_LARGE_ENTRY_PTR_H

#include "pas_compact_ptr.h"

PAS_BEGIN_EXTERN_C;

struct verse_heap_large_entry;
typedef struct verse_heap_large_entry verse_heap_large_entry;

PAS_DEFINE_COMPACT_PTR(verse_heap_large_entry,
                       verse_heap_compact_large_entry_ptr);

PAS_END_EXTERN_C;

#endif /* VERSE_HEAP_COMPACT_LARGE_ENTRY_PTR_H */

