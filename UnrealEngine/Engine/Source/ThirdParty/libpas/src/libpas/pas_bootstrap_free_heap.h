/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef PAS_BOOTSTRAP_FREE_HEAP_H
#define PAS_BOOTSTRAP_FREE_HEAP_H

#include "pas_allocation_config.h"
#include "pas_allocation_kind.h"
#include "pas_lock.h"
#include "pas_manually_decommittable_large_free_heap.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BOOTSTRAP_FREE_LIST_MINIMUM_SIZE 4u

PAS_API extern pas_manually_decommittable_large_free_heap pas_bootstrap_free_heap;
PAS_API extern size_t pas_bootstrap_free_heap_num_allocated_object_bytes;
PAS_API extern size_t pas_bootstrap_free_heap_num_allocated_object_bytes_peak;

PAS_API pas_allocation_result pas_bootstrap_free_heap_try_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind);
PAS_API pas_allocation_result pas_bootstrap_free_heap_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind);

static inline void* pas_bootstrap_free_heap_try_allocate_simple(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    return (void*)pas_bootstrap_free_heap_try_allocate(
        size, name, allocation_kind).begin;
}

static inline void* pas_bootstrap_free_heap_allocate_simple(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    return (void*)pas_bootstrap_free_heap_allocate(
        size, name, allocation_kind).begin;
}

PAS_API pas_allocation_result
pas_bootstrap_free_heap_try_allocate_with_manual_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind);

PAS_API pas_allocation_result
pas_bootstrap_free_heap_try_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind);

PAS_API pas_allocation_result
pas_bootstrap_free_heap_allocate_with_manual_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind);

PAS_API pas_allocation_result
pas_bootstrap_free_heap_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind);

PAS_API void pas_bootstrap_free_heap_deallocate(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind);

PAS_API size_t pas_bootstrap_free_heap_get_num_free_bytes(void);
PAS_API size_t pas_bootstrap_free_heap_get_num_decommitted_bytes(void);

PAS_API void* pas_bootstrap_free_heap_hold_lock_and_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind);
PAS_API void pas_bootstrap_free_heap_hold_lock_and_deallocate(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind);

PAS_API void* pas_bootstrap_free_heap_hold_lock_and_allocate_for_config(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind,
    void* arg);
PAS_API void pas_bootstrap_free_heap_hold_lock_and_deallocate_for_config(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    void* arg);

PAS_API void* pas_bootstrap_free_heap_allocate_simple_for_config(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind,
    void* arg);
PAS_API void pas_bootstrap_free_heap_deallocate_for_config(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    void* arg);

static inline void pas_bootstrap_free_heap_allocation_config_construct(
    pas_allocation_config* config,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    config->arg = NULL;

    switch (heap_lock_hold_mode) {
    case pas_lock_is_not_held:
        config->allocate = pas_bootstrap_free_heap_hold_lock_and_allocate_for_config;
        config->deallocate = pas_bootstrap_free_heap_hold_lock_and_deallocate_for_config;
        return;
        
    case pas_lock_is_held:
        config->allocate = pas_bootstrap_free_heap_allocate_simple_for_config;
        config->deallocate = pas_bootstrap_free_heap_deallocate_for_config;
        return;
    }
    
    PAS_ASSERT(!"Should not be reached");
}

PAS_API size_t pas_bootstrap_free_heap_decommit(void);
PAS_API bool pas_bootstrap_free_heap_scavenge_periodic(void);

PAS_END_EXTERN_C;

#endif /* PAS_BOOTSTRAP_HEAP_H */
