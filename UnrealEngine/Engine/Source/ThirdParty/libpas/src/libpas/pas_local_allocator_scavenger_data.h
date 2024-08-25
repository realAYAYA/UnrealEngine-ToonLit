/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H
#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H

#include "pas_local_allocator_kind.h"
#include "pas_local_allocator_location.h"
#include "pas_lock.h"
#include "ue_include/pas_local_allocator_scavenger_data_ue.h"

PAS_BEGIN_EXTERN_C;

#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_ENCODE_KIND_AND_LOCATION(kind, location) \
    ((uint8_t)((unsigned)(kind) | ((unsigned)(location) << 7)))

#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_DECODE_KIND(encoded) \
    ((pas_local_allocator_kind)((encoded) & 0x7f))

#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_DECODE_LOCATION(encoded) \
    ((pas_local_allocator_location)((encoded) >> 7))

#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_INITIALIZER(kind, location) \
    ((pas_local_allocator_scavenger_data){ \
         .is_in_use = false, \
         .should_stop_count = 0, \
         .dirty = false, \
         .encoded_kind_and_location = \
             PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_ENCODE_KIND_AND_LOCATION(kind, location), \
     })

PAS_API extern uint8_t pas_local_allocator_should_stop_count_for_suspend;

static inline pas_local_allocator_kind pas_local_allocator_scavenger_data_kind(
    pas_local_allocator_scavenger_data* data)
{
    return PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_DECODE_KIND(data->encoded_kind_and_location);
}

static inline pas_local_allocator_location pas_local_allocator_scavenger_data_location(
    pas_local_allocator_scavenger_data* data)
{
    return PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_DECODE_LOCATION(data->encoded_kind_and_location);
}

static inline void pas_local_allocator_scavenger_data_set_kind(pas_local_allocator_scavenger_data* data,
                                                               pas_local_allocator_kind kind)
{
    data->encoded_kind_and_location = PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_ENCODE_KIND_AND_LOCATION(
        kind, pas_local_allocator_scavenger_data_location(data));
}

static inline void pas_local_allocator_scavenger_data_construct(pas_local_allocator_scavenger_data* data,
                                                                pas_local_allocator_kind kind,
                                                                pas_local_allocator_location location)
{
    *data = PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_INITIALIZER(kind, location);
    PAS_ASSERT(sizeof(pas_local_allocator_scavenger_data) == 4);
    PAS_ASSERT(pas_local_allocator_scavenger_data_kind(data) == kind);
    PAS_ASSERT(pas_local_allocator_scavenger_data_location(data) == location);
}

static inline void pas_local_allocator_scavenger_data_did_use_for_allocation(
    pas_local_allocator_scavenger_data* data)
{
    data->dirty = true;
    data->should_stop_count = 0;
}

PAS_API bool pas_local_allocator_scavenger_data_is_stopped(pas_local_allocator_scavenger_data* data);

enum pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode {
    /* This causes the commit_if_necessary code to always succeed and always return true. */
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_in_use_with_no_locks_held_mode,

    /* This causes the commit_if_necessary code to sometimes fail to acquire the heap lock, and then it will
       return false. */
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_not_in_use_with_scavenger_lock_held_mode
};

typedef enum pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode;

PAS_API void pas_local_allocator_scavenger_data_commit_if_necessary_slow(
    pas_local_allocator_scavenger_data* data,
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode mode,
    pas_local_allocator_kind expected_kind);

PAS_API bool pas_local_allocator_scavenger_data_stop(
    pas_local_allocator_scavenger_data* data,
    pas_lock_lock_mode page_lock_mode);

PAS_API void pas_local_allocator_scavenger_data_prepare_to_decommit(pas_local_allocator_scavenger_data* data);

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H */

