/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_medium_page_header_object.h"

#include "pas_utility_heap.h"
#include "verse_heap_config.h"

#if PAS_ENABLE_VERSE

PAS_API verse_heap_medium_page_header_object* verse_heap_medium_page_header_object_create(void)
{
	verse_heap_medium_page_header_object* result;
	
	result = (verse_heap_medium_page_header_object*)
		pas_utility_heap_allocate(VERSE_HEAP_MEDIUM_SEGREGATED_HEADER_OBJECT_SIZE, "verse_heap_medium_page_header_object");

	PAS_ASSERT(verse_heap_page_base_for_page_header(&result->verse) == &result->segregated.base);
	PAS_ASSERT(verse_heap_page_header_for_segregated_page(&result->segregated) == &result->verse);

	return result;
}

PAS_API void verse_heap_medium_page_header_object_destroy(verse_heap_medium_page_header_object* header)
{
	pas_utility_heap_deallocate(header);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

