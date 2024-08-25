/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_PAGE_HEADER_UE_H
#define VERSE_HEAP_PAGE_HEADER_UE_H

#ifdef __cplusplus
extern "C" {
#endif

struct verse_heap_page_header;
typedef struct verse_heap_page_header verse_heap_page_header;

PAS_API void** verse_heap_page_header_lock_client_data(verse_heap_page_header* header);
PAS_API void verse_heap_page_header_unlock_client_data(verse_heap_page_header* header);

#ifdef __cplusplus
}
#endif

#endif /* VERSE_HEAP_PAGE_HEADER_UE_H */

