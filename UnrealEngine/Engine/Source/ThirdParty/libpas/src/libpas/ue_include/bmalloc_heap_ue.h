/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef BMALLOC_HEAP_UE_H
#define BMALLOC_HEAP_UE_H

#include "pas_reallocate_free_mode_ue.h"

#ifdef __cplusplus
extern "C" {
#endif

PAS_API void* bmalloc_allocate(size_t size);

PAS_API void* bmalloc_try_allocate_with_alignment(size_t size,
                                                  size_t alignment);
PAS_API void* bmalloc_allocate_with_alignment(size_t size,
                                              size_t alignment);
	
PAS_API void* bmalloc_try_reallocate_with_alignment(void* old_ptr, size_t new_size, size_t alignment,
                                                    pas_reallocate_free_mode free_mode);

PAS_API void* bmalloc_reallocate_with_alignment(void* old_ptr, size_t new_size, size_t alignment,
                                                pas_reallocate_free_mode free_mode);

PAS_API void bmalloc_deallocate(void*);

PAS_BAPI size_t bmalloc_get_allocation_size(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* BMALLOC_HEAP_UE_H */

