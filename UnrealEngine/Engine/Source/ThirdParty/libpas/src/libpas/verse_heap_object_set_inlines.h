/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_OBJECT_SET_INLINES_H
#define VERSE_HEAP_OBJECT_SET_INLINES_H

#include "bmalloc_heap.h"
#include "pas_large_utility_free_heap.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_size_directory.h"
#include "ue_include/verse_heap_config_ue.h"
#include "verse_heap_iterate_filter.h"
#include "verse_heap_large_entry.h"
#include "verse_heap_object_set.h"
#include "verse_heap_page_header_inlines.h"
#include "verse_heap_runtime_config.h"
#include <inttypes.h>

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE bool verse_heap_object_set_iterate_large_entries(verse_heap_object_set* set,
                                                                          bool (*callback)(void* object, void* arg),
                                                                          void* arg)
{
    size_t index;

	PAS_ASSERT(verse_heap_num_large_entries_for_iteration <= set->num_large_entries);

    for (index = 0; index < verse_heap_num_large_entries_for_iteration; ++index) {
        verse_heap_large_entry* entry;

		pas_heap_lock_lock();
        entry = verse_heap_compact_large_entry_ptr_load_non_null(set->large_entries + index);
		pas_heap_lock_unlock();

        if (!callback((void*)entry->begin, arg))
            return false;
    }

    return true;
}

typedef struct {
    unsigned* alloc_bits;
    void* page_boundary;
    unsigned* mark_bits_base;
    verse_heap_iterate_filter filter;
    void (*callback)(void* object, void* arg);
    void* arg;
} verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data;

static PAS_ALWAYS_INLINE unsigned verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_bit_source(
    size_t word_index,
    void* arg)
{
    verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data* data;
    data = (verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data*)arg;

    switch (data->filter) {
    case verse_heap_iterate_unmarked:
        return data->alloc_bits[word_index] & ~data->mark_bits_base[word_index];
    case verse_heap_iterate_marked:
        return data->alloc_bits[word_index] & data->mark_bits_base[word_index];
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static PAS_ALWAYS_INLINE bool verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_bit_callback(
    pas_found_bit_index index,
    void* arg)
{
    verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data* data;
    void* ptr;
    
    data = (verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data*)arg;

    PAS_ASSERT(index.did_succeed);

    ptr = (char*)data->page_boundary + (index.index << VERSE_HEAP_MIN_ALIGN_SHIFT);

    PAS_TESTING_ASSERT(verse_heap_is_marked(ptr) == (data->filter == verse_heap_iterate_marked));

    data->callback(ptr, data->arg);
    
    return true;
}

static PAS_ALWAYS_INLINE void verse_heap_object_set_iterate_segregated_exclusive_view_impl_small(
    verse_heap_object_set* set,
    pas_segregated_exclusive_view* view,
    unsigned* alloc_bits,
    void* page_boundary,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void *arg)
{
    const pas_segregated_page_config config = VERSE_HEAP_CONFIG.small_segregated_config;

    verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_data data;
    bool result;

    data.alloc_bits = alloc_bits;
    data.page_boundary = page_boundary;
    data.mark_bits_base = verse_heap_mark_bits_base_for_boundary(page_boundary);
    data.filter = filter;
    data.callback = callback;
    data.arg = arg;
    
    result = pas_bitvector_for_each_set_bit(
        verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_bit_source,
        0,
        pas_segregated_page_config_num_alloc_words(config),
        verse_heap_object_set_iterate_segregated_exclusive_view_impl_small_bit_callback,
        &data);

    PAS_ASSERT(result);
}

typedef struct {
    unsigned* alloc_bits;
    void* page_boundary;
    unsigned* mark_bits_base;
    verse_heap_iterate_filter filter;
    void (*callback)(void* object, void* arg);
    void* arg;
    pas_segregated_page_config config;
} verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data;

static PAS_ALWAYS_INLINE unsigned verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_bit_source(
    size_t word_index,
    void* arg)
{
    verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data* data;
    data = (verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data*)arg;

    return data->alloc_bits[word_index];
}

static PAS_ALWAYS_INLINE bool verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_bit_callback(
    pas_found_bit_index index,
    void* arg)
{
    verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data* data;
    uintptr_t offset;
    uintptr_t mark_bit_index;
    
    data = (verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data*)arg;

    PAS_ASSERT(index.did_succeed);
    PAS_ASSERT(data->config.base.min_align_shift >= VERSE_HEAP_MIN_ALIGN_SHIFT);

    offset = index.index << data->config.base.min_align_shift;
    mark_bit_index = index.index << (data->config.base.min_align_shift - VERSE_HEAP_MIN_ALIGN_SHIFT);

    switch (data->filter) {
    case verse_heap_iterate_unmarked:
        if (pas_bitvector_get(data->mark_bits_base, mark_bit_index))
            return true;
        break;
    case verse_heap_iterate_marked:
        if (!pas_bitvector_get(data->mark_bits_base, mark_bit_index))
            return true;
        break;
    }
    
    data->callback((char*)data->page_boundary + offset, data->arg);
    
    return true;
}

static PAS_ALWAYS_INLINE void verse_heap_object_set_iterate_segregated_exclusive_view_with_bits(
    verse_heap_object_set* set,
    pas_segregated_exclusive_view* view,
	unsigned* alloc_bits,
    void* page_boundary,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void *arg,
    pas_segregated_page_config config)
{
    verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_data data;
    bool result;

    if (config.kind == pas_segregated_page_config_kind_verse_small_segregated) {
        verse_heap_object_set_iterate_segregated_exclusive_view_impl_small(
            set, view, alloc_bits, page_boundary, filter, callback, arg);
        return;
    }
    
    data.alloc_bits = alloc_bits;
    data.page_boundary = page_boundary;
    data.mark_bits_base = verse_heap_mark_bits_base_for_boundary(page_boundary);
    data.filter = filter;
    data.callback = callback;
    data.arg = arg;
    data.config = config;
    
    result = pas_bitvector_for_each_set_bit(
        verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_bit_source,
        0,
        pas_segregated_page_config_num_alloc_words(config),
        verse_heap_object_set_iterate_segregated_exclusive_view_with_bits_bit_callback,
        &data);

    PAS_ASSERT(result);
}

static PAS_ALWAYS_INLINE void verse_heap_object_set_iterate_segregated_exclusive_view_with_config(
    verse_heap_object_set* set,
    pas_segregated_exclusive_view* view,
    void* page_boundary,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void *arg,
    pas_segregated_page_config config)
{
	unsigned local_alloc_bits[PAS_BITVECTOR_NUM_WORDS(config.num_alloc_bits)];
    pas_segregated_page* page;
	bool did_handle_iteration;
	unsigned* alloc_bits;
	bool should_free_alloc_bits;

    page = verse_heap_segregated_page_for_boundary(page_boundary, config.variant);

    PAS_TESTING_ASSERT(page->lock_ptr == &view->ownership_lock);

	did_handle_iteration = verse_heap_page_header_handle_iteration(
		verse_heap_page_header_for_segregated_page(page), verse_heap_current_iteration_state.version); 

    if (did_handle_iteration) {
		/* If a page is in use for allocation, then it should have iterated itself already. However, we might have
		   already set the is_in_use bit and then released the page lock to commit the page fully. In that case, no
		   allocation has happened in the page yet and the allocator hasn't done the iteration check. The alloc bits
		   still accurately reflect the page's state. So we can safely sneak in and run iteration. */
		if (PAS_ENABLE_TESTING
			&& page->is_in_use_for_allocation
			&& !page->is_committing_fully
			&& !verse_heap_page_header_for_segregated_page(page)->is_stashing_alloc_bits) {
			pas_log("Page unexpectedly in use for allocation (and it's not committing fully or stashing alloc bits); page = %p/%s\n",
					page, pas_page_kind_get_string(pas_page_base_get_kind(&page->base)));
			PAS_TESTING_ASSERT(!"Page was unexpectedly in use for allocation");
		}

		PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits);
		
		if (page->emptiness.num_non_empty_words_or_live_bytes) {
			memcpy(local_alloc_bits, page->alloc_bits, PAS_BITVECTOR_NUM_BYTES(config.num_alloc_bits));
			
			alloc_bits = local_alloc_bits;
		} else
			alloc_bits = NULL;
		
		should_free_alloc_bits = false;
	} else {
		alloc_bits = verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits;
		verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits = NULL;
		should_free_alloc_bits = true;
	}

	pas_lock_unlock(&view->ownership_lock);

	if (!alloc_bits)
		return;
	
	verse_heap_object_set_iterate_segregated_exclusive_view_with_bits(set, view, alloc_bits, page_boundary, filter, callback, arg, config);

	if (should_free_alloc_bits)
		bmalloc_deallocate(alloc_bits);
}

static PAS_ALWAYS_INLINE void verse_heap_object_set_iterate_segregated_exclusive_view(
    verse_heap_object_set* set,
    pas_segregated_exclusive_view* view,
    void* page_boundary,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void *arg)
{
    switch (pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base.page_config_kind) {
    case pas_segregated_page_config_kind_verse_small_segregated: {
        verse_heap_object_set_iterate_segregated_exclusive_view_with_config(
            set, view, page_boundary, filter, callback, arg, VERSE_HEAP_CONFIG.small_segregated_config);
        return;
	}

    case pas_segregated_page_config_kind_verse_medium_segregated:
        verse_heap_object_set_iterate_segregated_exclusive_view_with_config(
            set, view, page_boundary, filter, callback, arg, VERSE_HEAP_CONFIG.medium_segregated_config);
        return;

    default:
        PAS_ASSERT(!"Should not be reached");
        return;
    }
}

typedef struct {
    verse_heap_object_set* set;
    size_t end;
    verse_heap_iterate_filter filter;
    void (*callback)(void* object, void* arg);
    void* arg;
} verse_heap_object_set_iterate_views_data;

static PAS_ALWAYS_INLINE bool verse_heap_object_set_iterate_views_callback(
    pas_compact_atomic_segregated_exclusive_view_ptr* entry,
    size_t index,
    void* arg)
{
    verse_heap_object_set_iterate_views_data* data;
    pas_segregated_exclusive_view* view;

    data = (verse_heap_object_set_iterate_views_data*)arg;

    if (index >= data->end)
        return false;

    view = pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(entry);

    if (!view->is_owned)
        return true;
    
    pas_lock_lock(&view->ownership_lock);
    
    if (view->is_owned) {
        void* page_boundary;
        page_boundary = view->page_boundary;
        PAS_TESTING_ASSERT(page_boundary);
        
        verse_heap_object_set_iterate_segregated_exclusive_view(
            data->set, view, page_boundary, data->filter, data->callback, data->arg);
    } else
		pas_lock_unlock(&view->ownership_lock);
	
    return true;
}

typedef struct {
    verse_heap_iterate_filter filter;
    void (*callback)(void* object, void* arg);
    void* arg;
} verse_heap_object_set_iterate_iterate_large_entries_data;

static PAS_ALWAYS_INLINE bool verse_heap_object_set_iterate_iterate_large_entries_callback(void* object, void* arg)
{
    verse_heap_object_set_iterate_iterate_large_entries_data* data;
    data = (verse_heap_object_set_iterate_iterate_large_entries_data*)arg;
    switch (data->filter) {
    case verse_heap_iterate_unmarked:
        if (verse_heap_is_marked(object))
            return true;
        break;
    case verse_heap_iterate_marked:
        if (!verse_heap_is_marked(object))
            return true;
        break;
    }
    data->callback(object, data->arg);
    return true;
}

static PAS_ALWAYS_INLINE void verse_heap_object_set_iterate_range_inline(
    verse_heap_object_set* set,
    size_t begin,
    size_t end,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void* arg)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("iterating range %zu...%zu\n", begin, end);
    
    PAS_ASSERT(begin <= end);
    
    PAS_ASSERT(!verse_heap_is_sweeping);
    PAS_ASSERT(verse_heap_current_iteration_state.version == verse_heap_latest_version);
    PAS_ASSERT(set == verse_heap_current_iteration_state.set_being_iterated);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);

    if (!begin) {
        if (!end)
            return;
		verse_heap_object_set_iterate_iterate_large_entries_data data;
		data.filter = filter;
		data.callback = callback;
		data.arg = arg;
		verse_heap_object_set_iterate_large_entries(
			set, verse_heap_object_set_iterate_iterate_large_entries_callback, &data);
        begin = 1;
    }

    PAS_TESTING_ASSERT(begin);
    PAS_TESTING_ASSERT(end);
    --begin;
    --end;

    verse_heap_object_set_iterate_views_data data;
    data.set = set;
    data.end = end;
    data.filter = filter;
    data.callback = callback;
    data.arg = arg;
    verse_heap_view_vector_iterate(
        &set->views, begin, verse_heap_object_set_iterate_views_callback, &data);
}

static PAS_ALWAYS_INLINE bool verse_heap_object_set_contains_heap(verse_heap_object_set* set,
                                                                  pas_heap_runtime_config* generic_config)
{
    verse_heap_runtime_config* config;

    config = (verse_heap_runtime_config*)generic_config;

    return verse_heap_object_set_set_contains_set(&config->object_sets, set);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_OBJECT_SET_INLINES_H */

