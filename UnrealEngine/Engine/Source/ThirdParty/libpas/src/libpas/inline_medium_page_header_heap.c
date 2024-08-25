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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "inline_medium_page_header_heap.h"

#if PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER

#include "inline_medium_page_header_config.h"
#include "pas_deallocate.h"
#include "pas_try_allocate_intrinsic.h"

pas_intrinsic_heap_support inline_medium_page_header_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap inline_medium_page_header_common_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &inline_medium_page_header_common_primitive_heap,
        PAS_SIMPLE_TYPE_CREATE(1, 1),
        inline_medium_page_header_common_primitive_heap_support,
        INLINE_MEDIUM_PAGE_HEADER_CONFIG,
        &inline_medium_page_header_runtime_config);

pas_allocator_counts inline_medium_page_header_allocator_counts;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    test_allocate_common_primitive,
    INLINE_MEDIUM_PAGE_HEADER_CONFIG,
    &inline_medium_page_header_runtime_config,
    &inline_medium_page_header_allocator_counts,
    pas_allocation_result_crash_on_error,
    &inline_medium_page_header_common_primitive_heap,
    &inline_medium_page_header_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* inline_medium_page_header_allocate(size_t size)
{
    return (void*)test_allocate_common_primitive(size, 1).begin;
}

void inline_medium_page_header_deallocate(void* ptr)
{
    pas_deallocate(ptr, INLINE_MEDIUM_PAGE_HEADER_CONFIG);
}

#endif /* PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER */

#endif /* LIBPAS_ENABLED */

