/*
 * Copyright Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_GLOBAL_PHYSICAL_PAGE_SHARING_CACHE_H
#define PAS_GLOBAL_PHYSICAL_PAGE_SHARING_CACHE_H

#include "pas_heap_page_provider.h"
#include "pas_simple_large_free_heap.h"

PAS_BEGIN_EXTERN_C;

struct pas_physical_memory_transaction;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

PAS_API extern pas_simple_large_free_heap pas_global_physical_page_sharing_cache;

/* These calls give you memory that is tracked by the large sharing pool, is free, and may or may not be committed. */
PAS_API pas_allocation_result pas_global_physical_page_sharing_cache_try_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name);
PAS_API pas_allocation_result pas_global_physical_page_sharing_cache_allocate_with_alignment(
    size_t size, pas_alignment alignment, const char* name);

/* These calls give you committed memory that the large sharing pool thinks is allocated. Note that even the non-try
   variant may fail due to the need to rerun the commit transaction (but it won't fail for any other reason). */
PAS_API pas_allocation_result pas_global_physical_page_sharing_cache_try_allocate_committed_with_alignment(
	size_t size, pas_alignment alignment, const char* name, pas_physical_memory_transaction* transaction);
PAS_API pas_allocation_result pas_global_physical_page_sharing_cache_allocate_committed_with_alignment(
	size_t size, pas_alignment alignment, const char* name, pas_physical_memory_transaction* transaction);

pas_allocation_result pas_global_physical_page_sharing_cache_provider(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
	pas_primordial_page_state desired_state,
    void *arg);

PAS_END_EXTERN_C;

#endif /* PAS_GLOBAL_PHYSICAL_PAGE_SHARING_CACHE_H */

