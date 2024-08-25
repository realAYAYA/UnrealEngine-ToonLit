/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_PAGE_INLINES_H
#define PAS_SEGREGATED_PAGE_INLINES_H

#include "pas_config.h"
#include "pas_fd_stream.h"
#include "pas_log.h"
#include "pas_page_base_inlines.h"
#include "pas_segregated_deallocation_mode.h"
#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_page.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_handle_inlines.h"
#include "pas_thread_local_cache_node.h"
#include "verse_heap.h"
#include <inttypes.h>

/* FIXME: This cannot currently include verse headers because verse_heap_config.h depends on
   pas_heap_config_utils.h, which in turn depends on pas_segregated_page_inlines.h. That's weird, but
   it *just barely* doesn't matter right now. */

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE bool pas_segregated_page_initialize_full_use_counts(
    pas_segregated_size_directory* directory,
    pas_page_granule_use_count* use_counts,
    uintptr_t end_granule_index,
    pas_segregated_page_config page_config)
{
    uintptr_t num_granules;
    uintptr_t end_granule_offset;
    uintptr_t start_of_payload;
    uintptr_t end_of_payload;
    uintptr_t start_of_first_object;
    uintptr_t end_of_last_object;
    uintptr_t object_size;
    uintptr_t offset;
    pas_segregated_size_directory_data* data;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(directory);
    
    num_granules = page_config.base.page_size / page_config.base.granule_size;
    PAS_ASSERT(end_granule_index <= num_granules);

    end_granule_offset = end_granule_index * page_config.base.granule_size;

    pas_zero_memory(use_counts, end_granule_index * sizeof(pas_page_granule_use_count));
    
    /* If there are any bytes in the page not made available for allocation then make sure
       that the use counts know about it. */
    start_of_payload =
        page_config.exclusive_payload_offset;
    end_of_payload =
        pas_segregated_page_config_payload_end_offset_for_role(
            page_config, pas_segregated_page_exclusive_role);

    if (start_of_payload) {
        if (!end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, 0, pas_min_uintptr(start_of_payload, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);
    }

    if (start_of_payload > end_granule_offset)
        return false;

    data = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data);

    start_of_first_object = data->offset_from_page_boundary_to_first_object;
    end_of_last_object = data->offset_from_page_boundary_to_end_of_last_object;

    object_size = directory->object_size;
    
    for (offset = start_of_first_object; offset < end_of_last_object; offset += object_size) {
        if (offset >= end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, offset, pas_min_uintptr(offset + object_size, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);

        if (offset + object_size > end_granule_offset)
            return false;
    }

    if (page_config.base.page_size > end_of_payload) {
        if (end_of_payload >= end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, end_of_payload, pas_min_uintptr(page_config.base.page_size, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);

        if (page_config.base.page_size > end_granule_offset)
            return false;
    }
    
    return true;
}

PAS_API bool pas_segregated_page_lock_with_unbias_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr);

static PAS_ALWAYS_INLINE bool pas_segregated_page_lock_with_unbias_not_utility(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr)
{
    PAS_TESTING_ASSERT(lock_ptr);
    
    *held_lock = lock_ptr;
    
    if (PAS_LIKELY(pas_lock_try_lock(lock_ptr)))
        return lock_ptr == page->lock_ptr;

    return pas_segregated_page_lock_with_unbias_impl(page, held_lock, lock_ptr);
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_lock_with_unbias(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return true;
    }

    PAS_TESTING_ASSERT(lock_ptr);

    return pas_segregated_page_lock_with_unbias_not_utility(page, held_lock, lock_ptr);
}

static PAS_ALWAYS_INLINE void pas_segregated_page_lock(
    pas_segregated_page* page,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }
    
    for (;;) {
        pas_lock* lock_ptr;
        pas_lock* held_lock_ignored;
        
        lock_ptr = page->lock_ptr;

        if (pas_segregated_page_lock_with_unbias(page, &held_lock_ignored, lock_ptr, page_config))
            return;
        
        pas_lock_unlock(lock_ptr);
    }
}

static PAS_ALWAYS_INLINE void pas_segregated_page_unlock(
    pas_segregated_page* page,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }
    
    pas_lock_unlock(page->lock_ptr);
}

PAS_API pas_lock* pas_segregated_page_switch_lock_slow(
    pas_segregated_page* page,
    pas_lock* held_lock,
    pas_lock* page_lock);

static PAS_ALWAYS_INLINE void pas_segregated_page_switch_lock_impl(
    pas_segregated_page* page,
    pas_lock** held_lock)
{
    static const bool verbose = false;
    
    pas_lock* held_lock_value;
    pas_lock* page_lock;
    
    held_lock_value = *held_lock;
    page_lock = page->lock_ptr;

    PAS_TESTING_ASSERT(page_lock);
    
    if (PAS_LIKELY(held_lock_value == page_lock)) {
        if (verbose)
            pas_log("Getting the same lock as before (%p).\n", page_lock);
        return;
    }
    
    *held_lock = pas_segregated_page_switch_lock_slow(page, held_lock_value, page_lock);
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_switch_lock_with_mode(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock_lock_mode lock_mode,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return true;
    }

    switch (lock_mode) {
    case pas_lock_lock_mode_try_lock: {
        pas_lock* page_lock;
        pas_compiler_fence();
        page_lock = page->lock_ptr;
        PAS_TESTING_ASSERT(page_lock);
        if (*held_lock == page_lock) {
            pas_compiler_fence();
            return true;
        }
        pas_compiler_fence();
        if (*held_lock)
            pas_lock_unlock(*held_lock);
        pas_compiler_fence();
        for (;;) {
            pas_lock* new_page_lock;
            if (!pas_lock_try_lock(page_lock)) {
                *held_lock = NULL;
                pas_compiler_fence();
                return false;
            }
            new_page_lock = page->lock_ptr;
            if (page_lock == new_page_lock) {
                *held_lock = page_lock;
                pas_compiler_fence();
                return true;
            }
            pas_lock_unlock(page_lock);
            page_lock = new_page_lock;
        }
    }

    case pas_lock_lock_mode_lock: {
        pas_segregated_page_switch_lock_impl(page, held_lock);
        return true;
    } }
    PAS_ASSERT(!"Should not be reached");
    return true;
}

static PAS_ALWAYS_INLINE void pas_segregated_page_switch_lock(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_segregated_page_config page_config)
{
    bool result;

    PAS_ASSERT(page_config.base.is_enabled);
    
    result = pas_segregated_page_switch_lock_with_mode(
        page, held_lock, pas_lock_lock_mode_lock, page_config);
    PAS_ASSERT(result);
}

PAS_API void pas_segregated_page_switch_lock_and_rebias_while_ineligible_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_thread_local_cache_node* cache_node);

static PAS_ALWAYS_INLINE void
pas_segregated_page_switch_lock_and_rebias_while_ineligible(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_thread_local_cache_node* cache_node,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }

    if (pas_segregated_page_config_is_verse(page_config)) {
        pas_segregated_page_switch_lock(page, held_lock, page_config);
        return;
    }

    pas_segregated_page_switch_lock_and_rebias_while_ineligible_impl(
        page, held_lock, cache_node);
}

PAS_API void pas_segregated_page_verify_granules(pas_segregated_page* page);

PAS_API PAS_NO_RETURN PAS_USED void pas_segregated_page_deallocation_did_fail(uintptr_t begin);

typedef enum {
    pas_note_emptiness_clear_num_non_empty_words,
    pas_note_emptiness_keep_num_non_empty_words,
} pas_note_emptiness_action;

PAS_API void pas_segregated_page_note_emptiness_impl(pas_segregated_page* page, pas_note_emptiness_action);

static PAS_ALWAYS_INLINE void pas_segregated_page_note_partial_emptiness(pas_segregated_page* page)
{
    pas_segregated_page_note_emptiness_impl(page, pas_note_emptiness_keep_num_non_empty_words);
}

static PAS_ALWAYS_INLINE void pas_segregated_page_note_full_emptiness(pas_segregated_page* page,
                                                                      pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    if (pas_segregated_page_config_is_verse(page_config)) {
		/* If the page became empty and we hadn't cleared the client data then something is wrong. It's up to
		   the GC to clear client datas from pages with no marked objects after marking and before sweeping.
		
		   The way that this usually happens is that the client data is a map from object to stuff, and the GC
		   will prune the map based on liveness before sweep. If the map is empty, it gets deleted. asserts
		   that it must get deleted. */
		PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->client_data);
		
        /* GC needs to know that this page might now get scavenged, so that we don't attempt to do live
           object lookups in it.

           Here's a fun almost-problem: what if this happens from return_memory_to_page during a collection,
           and then the page gets scavenged?

           That would be a huge problem! Except it cannot happen:
           1) return_memory_to_page can only happen after we have started an allocator.
           2) The only paths to starting an allocator are paths that inevitably allocate one object.
           3) Objects can only be freed during sweeping.
           4) Sweeping happens during the part of collection where we no longer do find_allocated_object_start.

           Therefore, if we return_memory_to_page during the part of collection where this matters, then we
           will have allocated (2) but not yet freed (3) at least one object, so the page cannot become empty!

           FIXME: Maybe assert that we will not hit this path during the part of the cycle where this matters
           (i.e. marking)?

           FIXME: pas_segregated_exclusive_view_note_emptiness doesn't do anything if is_in_use_for_allocation.
           Is it a problem that we don't check is_in_use_for_allocation here? Probably not... */
        
        verse_heap_chunk_map_entry* entry_ptr;
        uintptr_t boundary;

        boundary = (uintptr_t)verse_heap_boundary_for_segregated_page(page, page_config.variant);
        entry_ptr = verse_heap_get_chunk_map_entry_ptr(boundary);
        switch (page_config.variant) {
        case pas_small_segregated_page_config_variant: {
            for (;;) {
                verse_heap_chunk_map_entry old_entry;
                unsigned bitvector;
                verse_heap_chunk_map_entry new_entry;
                
                verse_heap_chunk_map_entry_copy_atomically(&old_entry, entry_ptr);
                bitvector = verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(old_entry);
                pas_bitvector_set_in_one_word(
                    &bitvector,
                    pas_modulo_power_of_2(boundary, VERSE_HEAP_CHUNK_SIZE)
                    / VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE,
                    false);
                new_entry = verse_heap_chunk_map_entry_create_small_segregated(bitvector);
                
                if (verse_heap_chunk_map_entry_weak_cas_atomically(entry_ptr, old_entry, new_entry)) {
                    if (verbose) {
                        pas_log("Cleared small entry for page = %p, boundary = %p, entry_ptr = %p, old_entry = ",
                                page, (void*)boundary, entry_ptr);
                        verse_heap_chunk_map_entry_dump(old_entry, &pas_log_stream.base);
                        pas_log(", new_entry = ");
                        verse_heap_chunk_map_entry_dump(new_entry, &pas_log_stream.base);
                        pas_log("\n");
                    }
                    break;
                }
            }
            break;
        }
        case pas_medium_segregated_page_config_variant: {
            verse_heap_chunk_map_entry old_entry;
            verse_heap_chunk_map_entry entry;
			old_entry = *entry_ptr;
			PAS_ASSERT(verse_heap_chunk_map_entry_is_medium_segregated(old_entry));
			PAS_ASSERT(&verse_heap_chunk_map_entry_medium_segregated_header_object(old_entry)->segregated == page);
			PAS_ASSERT(verse_heap_chunk_map_entry_medium_segregated_empty_mode(old_entry) == pas_is_not_empty);
			entry = verse_heap_chunk_map_entry_create_medium_segregated(
				verse_heap_chunk_map_entry_medium_segregated_header_object(old_entry), pas_is_empty);
            if (verbose) {
                pas_log("Clearing medium entry for page = %p, boundary = %p, entry_ptr = %p, old_entry = ",
                        page, (void*)boundary, entry_ptr);
                verse_heap_chunk_map_entry_dump(old_entry, &pas_log_stream.base);
                pas_log(", new_entry = ");
                verse_heap_chunk_map_entry_dump(entry, &pas_log_stream.base);
                pas_log("\n");
            }
            verse_heap_chunk_map_entry_copy_atomically(entry_ptr, &entry);
            break;
        } }
    }
    pas_segregated_page_note_emptiness_impl(page, pas_note_emptiness_clear_num_non_empty_words);
}

static PAS_ALWAYS_INLINE void
pas_segregated_page_deallocate_with_page(pas_segregated_page* page,
                                         uintptr_t begin,
                                         pas_segregated_deallocation_mode deallocation_mode,
                                         pas_thread_local_cache* thread_local_cache,
                                         pas_segregated_page_config page_config,
                                         pas_segregated_page_role role)
{
    static const bool verbose = false;
    static const bool count_things = false;

    static uint64_t count_exclusive;
    static uint64_t count_partial;
    
    size_t bit_index_unmasked;
    size_t word_index;
    
    unsigned word;
    unsigned new_word;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (verbose) {
        pas_log("Freeing %p in %p(%s), num_non_empty_words_or_live_bytes = %llu\n",
                (void*)begin, page,
                pas_segregated_page_config_kind_get_string(page_config.kind),
                (unsigned long long)page->emptiness.num_non_empty_words_or_live_bytes);
    }

    bit_index_unmasked = begin >> page_config.base.min_align_shift;
    
    word_index = pas_modulo_power_of_2(
        (begin >> (page_config.base.min_align_shift + PAS_BITVECTOR_WORD_SHIFT)),
        page_config.base.page_size >> (page_config.base.min_align_shift + PAS_BITVECTOR_WORD_SHIFT));

    if (count_things) {
        pas_segregated_view owner;
        owner = page->owner;
        if (pas_segregated_view_is_shared_handle(owner))
            count_partial++;
        else
            count_exclusive++;
        pas_log("frees in partial = %llu, frees in exclusive = %llu.\n",
                (unsigned long long)count_partial, (unsigned long long)count_exclusive);
    }
    
    word = page->alloc_bits[word_index];

    if (page_config.check_deallocation) {
#if !PAS_ARM && !PAS_RISCV && !PAS_COMPILER(MSVC)
        new_word = word;
        
        asm volatile (
            "btrl %1, %0\n\t"
            "jc 0f\n\t"
            "movq %2, %%rdi\n\t"
#if PAS_OS(DARWIN)
            "call _pas_segregated_page_deallocation_did_fail\n\t"
#else
            "call pas_segregated_page_deallocation_did_fail\n\t"
#endif
            "0:"
            : "+r"(new_word)
            : "r"((unsigned)bit_index_unmasked), "r"(begin)
            : "memory");
#else /* !PAS_ARM && !PAS_RISCV -> so PAS_ARM or PAS_RISCV */
        unsigned bit_mask;
        bit_mask = PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
        
        if (PAS_UNLIKELY(!(word & bit_mask)))
            pas_segregated_page_deallocation_did_fail(begin);
        
        new_word = word & ~bit_mask;
#endif /* !PAS_ARM && !PAS_RISCV-> so end of PAS_ARM or PAS_RISCV */
    } else
        new_word = word & ~PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
    
    page->alloc_bits[word_index] = new_word;

    if (verbose)
        pas_log("at word_index = %zu, new_word = %u\n", word_index, new_word);

    if (!pas_segregated_page_config_enable_empty_word_eligibility_optimization_for_role(page_config, role)
        || !new_word) {
        pas_segregated_view owner;
        owner = page->owner;

        switch (role) {
        case pas_segregated_page_exclusive_role: {
            if (pas_segregated_view_get_kind(owner) != pas_segregated_exclusive_view_kind) {
                PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(owner));
                if (verbose)
                    pas_log("Notifying exclusive-ish eligibility on view %p.\n", owner);
                /* NOTE: If this decides to cache the view then it's possible that we will release and
                   then reacquire this page's lock. */
                pas_segregated_exclusive_view_note_eligibility(
                    pas_segregated_view_get_exclusive(owner),
                    page, deallocation_mode, thread_local_cache, page_config);
            }
            break;
        }
            
        case pas_segregated_page_shared_role: {
            pas_segregated_shared_handle* shared_handle;
            pas_segregated_partial_view* partial_view;
            size_t offset_in_page;
            size_t bit_index;
            
            offset_in_page = pas_modulo_power_of_2(begin, page_config.base.page_size);
            bit_index = offset_in_page >> page_config.base.min_align_shift;
            
            shared_handle = pas_segregated_view_get_shared_handle(owner);
            
            partial_view = pas_segregated_shared_handle_partial_view_for_index(
                shared_handle, bit_index, page_config);
            
            if (verbose)
                pas_log("Notifying partial eligibility on view %p.\n", partial_view);
            
            if (!partial_view->eligibility_has_been_noted)
                pas_segregated_partial_view_note_eligibility(partial_view, page);
            break;
        } }
    }

    if (page_config.base.page_size > page_config.base.granule_size) {
        /* This is the partial decommit case. It's intended for medium pages. It requires doing
           more work, but it's a bounded amount of work, and it only happens when freeing
           medium objects. */

        uintptr_t object_size;
        pas_segregated_view owner;
        size_t offset_in_page;
        size_t bit_index;
        bool did_find_empty_granule;

        offset_in_page = pas_modulo_power_of_2(begin, page_config.base.page_size);
        bit_index = offset_in_page >> page_config.base.min_align_shift;
        owner = page->owner;

        if (pas_segregated_view_is_some_exclusive(owner))
            object_size = page->object_size;
        else {
            object_size = pas_compact_segregated_size_directory_ptr_load_non_null(
                &pas_segregated_shared_handle_partial_view_for_index(
                    pas_segregated_view_get_shared_handle(owner),
                    bit_index,
                    page_config)->directory)->object_size;
        }

        did_find_empty_granule = pas_page_base_free_granule_uses_in_range(
            pas_segregated_page_get_granule_use_counts(page, page_config),
            offset_in_page,
            offset_in_page + object_size,
            page_config.base);

        if (pas_segregated_page_deallocate_should_verify_granules)
            pas_segregated_page_verify_granules(page);

        if (did_find_empty_granule)
            pas_segregated_page_note_partial_emptiness(page);
    }

    /* We support this path for Verse because it's how we handle pas_local_allocator_stop and because
       we use this for the generic case of sweep. */
    if (pas_segregated_page_config_is_verse(page_config)) {
        uintptr_t num_live_bytes;
        uintptr_t object_size;
        num_live_bytes = page->emptiness.num_non_empty_words_or_live_bytes;
        object_size = page->object_size;
        PAS_TESTING_ASSERT(num_live_bytes >= object_size);
        num_live_bytes -= object_size;
        if (!num_live_bytes)
            pas_segregated_page_note_full_emptiness(page, page_config);
        else
            page->emptiness.num_non_empty_words_or_live_bytes = num_live_bytes;
    } else {
        if (!new_word) {
            PAS_TESTING_ASSERT(page->emptiness.num_non_empty_words_or_live_bytes);
            uintptr_t num_non_empty_words = page->emptiness.num_non_empty_words_or_live_bytes;
            if (!--num_non_empty_words) {
                /* This has to happen last since it effectively unlocks the lock. That's due to
                   the things that happen in switch_lock_and_try_to_take_bias. Specifically, its
                   reliance on the fully_empty bit. */
                pas_segregated_page_note_full_emptiness(page, page_config);
            } else
                page->emptiness.num_non_empty_words_or_live_bytes = num_non_empty_words;
        }
    }
}

static PAS_ALWAYS_INLINE void pas_segregated_page_deallocate(
    uintptr_t begin,
    pas_lock** held_lock,
    pas_segregated_deallocation_mode deallocation_mode,
    pas_thread_local_cache* thread_local_cache,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    pas_segregated_page* page;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    page = pas_segregated_page_for_address_and_page_config(begin, page_config);
    pas_segregated_page_switch_lock(page, held_lock, page_config);
    pas_segregated_page_deallocate_with_page(
        page, begin, deallocation_mode, thread_local_cache, page_config, role);
}

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_page_get_directory_for_address_in_page(pas_segregated_page* page,
                                                      uintptr_t begin,
                                                      pas_segregated_page_config page_config,
                                                      pas_segregated_page_role role)
{
    pas_segregated_view owning_view;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    owning_view = page->owner;

    switch (role) {
    case pas_segregated_page_exclusive_role:
        return pas_compact_segregated_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(owning_view)->directory);

    case pas_segregated_page_shared_role:
        return pas_compact_segregated_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view), begin, page_config)->directory);
    }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_page_get_directory_for_address_and_page_config(uintptr_t begin,
                                                              pas_segregated_page_config page_config,
                                                              pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_directory_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config, role);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_in_page(pas_segregated_page* page,
                                                        uintptr_t begin,
                                                        pas_segregated_page_config page_config,
                                                        pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    switch (role) {
    case pas_segregated_page_exclusive_role:
        PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(page->owner));
        return page->object_size;

    case pas_segregated_page_shared_role: {
        pas_segregated_view owning_view;
        
        owning_view = page->owner;
    
        return pas_compact_segregated_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view),
                begin, page_config)->directory)->object_size;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_and_page_config(
    uintptr_t begin,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_object_size_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config, role);
}

static PAS_ALWAYS_INLINE void pas_segregated_page_log_or_deallocate(
    uintptr_t begin,
    pas_thread_local_cache* cache,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    pas_segregated_deallocation_logging_mode mode;
    
    mode = pas_segregated_page_config_logging_mode_for_role(page_config, role);

    if (!pas_segregated_deallocation_logging_mode_does_logging(mode)) {
        pas_lock* held_lock;
        held_lock = NULL;
        pas_segregated_page_deallocate(
            begin, &held_lock, pas_segregated_deallocation_direct_mode, NULL, page_config, role);
        pas_lock_switch(&held_lock, NULL);
        return;
    }

    /* This check happens here because if logging is disabled then whether we check deallocation is governed
       by the check_deallocation flag. */
    if (pas_segregated_deallocation_logging_mode_is_checked(mode)) {
        if (PAS_UNLIKELY(!pas_segregated_page_is_allocated(begin, page_config)))
            pas_deallocation_did_fail("Page bit not set", begin);
    }

    if (pas_segregated_deallocation_logging_mode_is_size_aware(mode)) {
        pas_thread_local_cache_append_deallocation_with_size(
            cache, begin,
            pas_segregated_page_get_object_size_for_address_and_page_config(begin, page_config, role),
            pas_segregated_page_config_kind_and_role_create(page_config.kind, role));
        return;
    }
    
    pas_thread_local_cache_append_deallocation(
        cache, begin, pas_segregated_page_config_kind_and_role_create(page_config.kind, role));
}

/* Given some address in a page, returns the base address of the object or 0 if no such object is allocated.

   If the pointer points within bounds of a live object, then this function works as advertised, without any
   caveats. You can rely on this function regardless of page_config in that case. You don't have to hold any
   locks.

   Pointing at the end of the object will not reliably be in bounds. It might be in bounds if there was a
   size-class round-up that happened. That's likely, but you cannot rely on it.

   If the poiner points at a dead object, then this function may either work (reliably return 0) or do crazy
   stuff (like crash or who knows).

   It will work reliably if the following holds:

   - Decommitted pages are still readable and at worst report zero.

   - Pages have an inline header, or there is some way to know that the header is at worst zero and finding it
     never crashes or does weird stuff.

   This is intended to work even if you do it concurrently to the scavenger.

   This will always work for pages that are definitely committed. So if you somehow know that the page you
   are pointing at is definitely committed, then this will reliably return 0 for dead objects in that page.

   At worst, this will report dead memory as being allocated. This will happen for memory in pages taken by
   local allocators. So, you can use this precisely if you either:

   - know that local allocators are stopped, or

   - you know something about the memory that is held by local allocators, for example by instrumenting
     refill somehow. */
static PAS_ALWAYS_INLINE uintptr_t pas_segregated_page_try_find_allocated_object_start_with_page(
    pas_segregated_page* page,
    uintptr_t inner_ptr,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    static const bool verbose = false;
    
    uintptr_t object_size;
    uintptr_t remaining_size_in_bits;
    uintptr_t bit_index;
    uintptr_t byte_index;
    intptr_t word64_offset;
    uintptr_t bit_index_in_byte;
    uintptr_t boundary;

    if (verbose)
        pas_log("considering inner_ptr = %zx\n", inner_ptr);
    
    switch (role) {
    case pas_segregated_page_exclusive_role:
        object_size = page->object_size;
        PAS_ASSERT(pas_is_aligned(object_size, pas_segregated_page_config_min_align(page_config)));
        break;

    case pas_segregated_page_shared_role:
        /* For now we just need the object size to cut off searches. For partial views, getting the object size would
           require doing some amount of work. So we just cut off the search based on the worst case and then
           we get the real object size once we know the real object start. */
        object_size = page_config.base.max_object_size;
        break;
    }

    boundary = pas_round_down_to_power_of_2(inner_ptr, page_config.base.page_size);
    PAS_TESTING_ASSERT((void*)boundary == pas_segregated_page_boundary(page, page_config));
    
    remaining_size_in_bits = object_size >> page_config.base.min_align_shift;
    bit_index = (inner_ptr - boundary) >> page_config.base.min_align_shift;
    bit_index_in_byte = bit_index & 7;
    byte_index = bit_index >> 3;
    word64_offset = (intptr_t)byte_index - 7;

    /* The arithmetic here is very confusing! There is a sneaky "off by one" correction that happens and
       seems surely wrong until you think about it deeply.

       Let's consider the first iteration of the loop.
       
       We'd like to find the closest bit to the left of some bit. We identified the byte that it's in, that's
       byte_index. Then we misaligned-load a 64-bit word that includes that byte as its last. Let's consider some
       examples of what could happen next, depending on bit_index_in_byte.

       Say that bit_index_in_byte == 0. This means that the 7 highest-order bits in the 64-bit word are meaningless
       to us. We're looking for the highest-order set bit in the lowest 57 bits of the word. So, loading that one word
       will give us visibility into 57 bits at once.

       Say that bit_index_in_byte == 7. This means that all of the 64 bits we have loaded are meaningful. We will
       consider all of them. So, loading 64 bits gives us visibility into 64 bits at once.

       Let those two weird arithmetic outcomes be your guide as you read this code.

       In case you're wondering why I'm misaligned-loading: because it's hella fast. Modern CPUs run misaligned loads
       at the same speed as aligned ones. 57 bits at 16 byte minalign is 912 bytes, so in the common case of objects
       no larger than that, this loop will complete in one iteration. Maybe even peeling the first iteration by hand
       could make this even faster. */
    for (;;) {
        uintptr_t num_bits_just_considered;
        uint64_t* word64_ptr;
        uint64_t word64;

        if (verbose) {
            pas_log("word64_offset = %zu\n", word64_offset);
            pas_log("remaining_size_in_bits = %zu\n", remaining_size_in_bits);
            pas_log("bit_index = %zu\n", bit_index);
            pas_log("bit_index_in_byte = %zu\n", bit_index_in_byte);
        }

        /* Thank goodness for the fact that we're guaranteed to at least have some kind of bytes to load without
           faulting before the beginning of alloc_bits!

           Oh yeah, did I mention that none of this works on big endian? */
        word64_ptr = (uint64_t*)((char*)page->alloc_bits + word64_offset);
        PAS_TESTING_ASSERT((uintptr_t)word64_ptr >= (uintptr_t)page);
        memcpy(&word64, word64_ptr, sizeof(uint64_t));

        if (verbose)
            pas_log("word64 = %" PRIu64 "\n", word64);
        
        /* Move the "suspect" alloc bit into the last position in the word. */
        word64 <<= 7 - bit_index_in_byte;
        if (word64) {
            uintptr_t distance_to_allocation;
            uintptr_t offset;
            uintptr_t result;
            distance_to_allocation = pas_count_leading_zeroes64(word64);
            if (verbose)
                pas_log("distance_to_allocation = %zu\n", distance_to_allocation);
            offset = (bit_index - distance_to_allocation) << page_config.base.min_align_shift;
            if (offset < pas_segregated_page_config_payload_offset_for_role(page_config, role)) {
                if (verbose)
                    pas_log("below payload begin\n");
                return 0;
            }
            if (offset >= pas_segregated_page_config_payload_end_offset_for_role(page_config, role)) {
                if (verbose)
                    pas_log("above payload end\n");
                return 0;
            }
            result = boundary + offset;
            
            switch (role) {
            case pas_segregated_page_exclusive_role:
                break;

            case pas_segregated_page_shared_role:
                object_size = pas_segregated_page_get_object_size_for_address_in_page(
                    page, inner_ptr, page_config, pas_segregated_page_shared_role);
                break;
            }

            PAS_TESTING_ASSERT(result <= inner_ptr);

            if (verbose)
                pas_log("result = %zx, object_size = %zu\n", result, object_size);
            
            if (inner_ptr - result >= object_size) {
                if (verbose) {
                    pas_log("bigger than object_size\n");
                }
                return 0;
            }
            
            if (verbose)
                pas_log("returning result = %zx\n", result);
            return result;
        }

        num_bits_just_considered = 57 + bit_index_in_byte;

        /* Have we considered all of our remaining bits? */
        if (remaining_size_in_bits <= num_bits_just_considered) {
            if (verbose)
                pas_log("not enough remaining bits\n");
            return 0;
        }

        /* Were we already falling off the beginning of the alloc_bits? */
        if (word64_offset <= 0) {
            if (verbose)
                pas_log("negative word offset\n");
            return 0;
        }

        word64_offset -= 8;
        remaining_size_in_bits -= num_bits_just_considered;
        PAS_TESTING_ASSERT(bit_index >= num_bits_just_considered);
        bit_index -= num_bits_just_considered;
        bit_index_in_byte = 7;
    }
}

static PAS_ALWAYS_INLINE uintptr_t pas_segregated_page_try_find_allocated_object_start(
    uintptr_t inner_ptr,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    return pas_segregated_page_try_find_allocated_object_start_with_page(
        pas_segregated_page_for_address_and_page_config(inner_ptr, page_config), inner_ptr, page_config, role);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_INLINES_H */

