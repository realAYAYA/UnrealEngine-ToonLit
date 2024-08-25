/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_COMPACT_MARK_BITS_PAGE_COMMIT_CONTROLLER_PTR_H
#define VERSE_HEAP_COMPACT_MARK_BITS_PAGE_COMMIT_CONTROLLER_PTR_H

#include "pas_compact_ptr.h"

PAS_BEGIN_EXTERN_C;

struct verse_heap_mark_bits_page_commit_controller;
typedef struct verse_heap_mark_bits_page_commit_controller verse_heap_mark_bits_page_commit_controller;

PAS_DEFINE_COMPACT_PTR(verse_heap_mark_bits_page_commit_controller,
                       verse_heap_compact_mark_bits_page_commit_controller_ptr);

PAS_END_EXTERN_C;

#endif /* VERSE_HEAP_COMPACT_MARK_BITS_PAGE_COMMIT_CONTROLLER_PTR_H */

