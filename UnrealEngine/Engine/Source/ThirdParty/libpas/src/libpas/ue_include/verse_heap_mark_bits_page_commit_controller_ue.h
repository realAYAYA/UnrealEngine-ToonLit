/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_UE_H
#define VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_UE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forces all mark bit pages to be committed and ensures that they cannot become decommitted.  */
PAS_API void verse_heap_mark_bits_page_commit_controller_lock(void);

/* Relinquishes the requirement that mark bit pages are committed. */
PAS_API void verse_heap_mark_bits_page_commit_controller_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_UE_H */

