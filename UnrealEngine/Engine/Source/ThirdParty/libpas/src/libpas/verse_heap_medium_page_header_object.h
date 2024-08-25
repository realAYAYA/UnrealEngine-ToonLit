/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_MEDIUM_PAGE_HEADER_OBJECT_H
#define VERSE_HEAP_MEDIUM_PAGE_HEADER_OBJECT_H

#include "pas_segregated_page.h"
#include "verse_heap_page_header.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct verse_heap_medium_page_header_object;
typedef struct verse_heap_medium_page_header_object verse_heap_medium_page_header_object;

struct verse_heap_medium_page_header_object {
	uintptr_t boundary;
	verse_heap_page_header verse;
	pas_segregated_page segregated;
};

/* These allocation functions need to be called with the heap lock held. That could be fixed if we had a variant of the utility heap
   that operated without heap lock and used normal TLCs. It wouldn't be super hard to make such a thing. One annoying feature of that
   would be that you'd have to pass physical memory transactions down to it, yuck. */

/* Returns an uninitialized page header object. */
PAS_API verse_heap_medium_page_header_object* verse_heap_medium_page_header_object_create(void);
PAS_API void verse_heap_medium_page_header_object_destroy(verse_heap_medium_page_header_object* header);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_MEDIUM_PAGE_HEADER_OBJECT_H */

