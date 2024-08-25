/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_large_map.h"

#include "pas_large_heap.h"
#include "pas_large_utility_free_heap.h"

pas_large_map_hashtable pas_large_map_hashtable_instance = PAS_HASHTABLE_INITIALIZER;
pas_large_map_hashtable_in_flux_stash pas_large_map_hashtable_instance_in_flux_stash;

pas_large_map_entry pas_large_map_find(uintptr_t begin)
{
    pas_heap_lock_assert_held();

    return pas_large_map_hashtable_get(&pas_large_map_hashtable_instance, begin);
}

void pas_large_map_add(pas_large_map_entry entry)
{
    static const bool verbose = false;
    
    pas_heap_lock_assert_held();

    if (verbose)
        pas_log("adding %p...%p, heap = %p.\n", (void*)entry.begin, (void*)entry.end, entry.heap);

    pas_large_map_hashtable_add_new(
        &pas_large_map_hashtable_instance, entry,
        &pas_large_map_hashtable_instance_in_flux_stash,
        &pas_large_utility_free_heap_allocation_config);
}

pas_large_map_entry pas_large_map_take(uintptr_t begin)
{
    static const bool verbose = false;
    
    pas_heap_lock_assert_held();

    if (verbose)
        pas_log("taking begin = %p.\n", (void*)begin);

    return pas_large_map_hashtable_take(
        &pas_large_map_hashtable_instance, begin,
        &pas_large_map_hashtable_instance_in_flux_stash,
        &pas_large_utility_free_heap_allocation_config);
}

bool pas_large_map_for_each_entry(pas_large_map_for_each_entry_callback callback,
                                  void* arg)
{
    size_t index;

    for (index = pas_large_map_hashtable_entry_index_end(&pas_large_map_hashtable_instance);
         index--;) {
        pas_large_map_entry entry;
        entry = *pas_large_map_hashtable_entry_at_index(&pas_large_map_hashtable_instance,
                                                        index);
        if (pas_large_map_entry_is_empty_or_deleted(entry))
            continue;
        if (!callback(entry, arg))
            return false;
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
