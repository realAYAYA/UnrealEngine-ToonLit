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

#include "outline_medium_page_header_heap.h"

#if PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER

#include "outline_medium_page_header_config.h"
#include "pas_deallocate.h"
#include "pas_try_allocate_intrinsic.h"

pas_intrinsic_heap_support outline_medium_page_header_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap outline_medium_page_header_common_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &outline_medium_page_header_common_primitive_heap,
        PAS_SIMPLE_TYPE_CREATE(1, 1),
        outline_medium_page_header_common_primitive_heap_support,
        OUTLINE_MEDIUM_PAGE_HEADER_CONFIG,
        &outline_medium_page_header_runtime_config);

pas_allocator_counts outline_medium_page_header_allocator_counts;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    test_allocate_common_primitive,
    OUTLINE_MEDIUM_PAGE_HEADER_CONFIG,
    &outline_medium_page_header_runtime_config,
    &outline_medium_page_header_allocator_counts,
    pas_allocation_result_crash_on_error,
    &outline_medium_page_header_common_primitive_heap,
    &outline_medium_page_header_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* outline_medium_page_header_allocate(size_t size)
{
    return (void*)test_allocate_common_primitive(size, 1).begin;
}

void outline_medium_page_header_deallocate(void* ptr)
{
    pas_deallocate(ptr, OUTLINE_MEDIUM_PAGE_HEADER_CONFIG);
}

#endif /* PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER */

#endif /* LIBPAS_ENABLED */

