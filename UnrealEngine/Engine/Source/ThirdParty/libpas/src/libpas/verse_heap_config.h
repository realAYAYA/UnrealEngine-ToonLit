/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_CONFIG_H
#define VERSE_HEAP_CONFIG_H

#include "pas_heap_config_utils.h"
#include "verse_heap_chunk_map.h"
#include "verse_heap_medium_page_header_object.h"
#include "verse_heap_page_header.h"
#include "verse_heap_type.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

#define VERSE_HEAP_SMALL_PAGE_MAX_ELIGIBLE_OCCUPANCY ((double)0.7)

#define VERSE_HEAP_SMALL_SEGREGATED_GRANULE_SIZE VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE
#define VERSE_HEAP_SMALL_SEGREGATED_HEADER_SIZE \
    (sizeof(verse_heap_page_header) + \
     PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE( \
        VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT, \
        VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE, \
        VERSE_HEAP_SMALL_SEGREGATED_GRANULE_SIZE))
#define VERSE_HEAP_SMALL_SEGREGATED_PAYLOAD_SIZE \
    (VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE - VERSE_HEAP_SMALL_SEGREGATED_HEADER_SIZE)
#define VERSE_HEAP_SMALL_SEGREGATED_MAX_OBJECT_SIZE \
    PAS_MAX_OBJECT_SIZE(VERSE_HEAP_SMALL_SEGREGATED_PAYLOAD_SIZE)
#define VERSE_HEAP_SMALL_SEGREGATED_WASTEAGE_HANDICAP PAS_SMALL_PAGE_HANDICAP

/* We make medium pages larger than normal because:

   1. We don't have bitfit.

   2. We want to match chunk size.

   Also note the MAX_OBJECT_SIZE - we intentionally allow mediums to be used even if it means just one
   object.*/
#define VERSE_HEAP_MEDIUM_SEGREGATED_GRANULE_SIZE PAS_GRANULE_DEFAULT_SIZE
#define VERSE_HEAP_MEDIUM_SEGREGATED_HEADER_SIZE VERSE_HEAP_PAGE_SIZE
#define VERSE_HEAP_MEDIUM_SEGREGATED_PAYLOAD_SIZE \
    (VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE - VERSE_HEAP_MEDIUM_SEGREGATED_HEADER_SIZE)
#define VERSE_HEAP_MEDIUM_SEGREGATED_MAX_OBJECT_SIZE VERSE_HEAP_MEDIUM_SEGREGATED_PAYLOAD_SIZE
#define VERSE_HEAP_MEDIUM_SEGREGATED_WASTEAGE_HANDICAP PAS_MEDIUM_PAGE_HANDICAP

#define VERSE_HEAP_MEDIUM_SEGREGATED_HEADER_OBJECT_SIZE \
    (PAS_OFFSETOF(verse_heap_medium_page_header_object, segregated) + \
     PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE( \
        VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT, \
        VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE, \
        VERSE_HEAP_MEDIUM_SEGREGATED_GRANULE_SIZE))

static PAS_ALWAYS_INLINE pas_page_base* verse_heap_page_base_for_page_header(verse_heap_page_header* header)
{
    return (pas_page_base*)(header + 1);
}

static PAS_ALWAYS_INLINE verse_heap_page_header* verse_heap_page_header_for_page_base(pas_page_base* page)
{
    return ((verse_heap_page_header*)page) - 1;
}

static PAS_ALWAYS_INLINE pas_segregated_page* verse_heap_segregated_page_for_page_header(
    verse_heap_page_header* header)
{
    return pas_page_base_get_segregated(verse_heap_page_base_for_page_header(header));
}

static PAS_ALWAYS_INLINE verse_heap_page_header* verse_heap_page_header_for_segregated_page(
    pas_segregated_page* page)
{
    return verse_heap_page_header_for_page_base(&page->base);
}

static PAS_ALWAYS_INLINE verse_heap_page_header* verse_heap_page_header_for_boundary(
    void* boundary, pas_segregated_page_config_variant variant)
{
    if (variant == pas_medium_segregated_page_config_variant)
        return &verse_heap_chunk_map_entry_medium_segregated_header_object(verse_heap_get_chunk_map_entry((uintptr_t)boundary))->verse;
    return (verse_heap_page_header*)boundary;
}

static PAS_ALWAYS_INLINE void* verse_heap_boundary_for_page_header(
    verse_heap_page_header* header, pas_segregated_page_config_variant variant)
{
    if (variant == pas_medium_segregated_page_config_variant) {
		verse_heap_medium_page_header_object* header_object;
		header_object = (verse_heap_medium_page_header_object*)((uintptr_t)header - PAS_OFFSETOF(verse_heap_medium_page_header_object, verse));
		return (void*)header_object->boundary;
	}
    return header;
}

static PAS_ALWAYS_INLINE pas_page_base* verse_heap_page_base_for_boundary(
    void* boundary, pas_segregated_page_config_variant variant)
{
    return verse_heap_page_base_for_page_header(verse_heap_page_header_for_boundary(boundary, variant));
}

static PAS_ALWAYS_INLINE void* verse_heap_boundary_for_page_base(
    pas_page_base* page, pas_segregated_page_config_variant variant)
{
    return verse_heap_boundary_for_page_header(verse_heap_page_header_for_page_base(page), variant);
}

static PAS_ALWAYS_INLINE pas_segregated_page* verse_heap_segregated_page_for_boundary(
    void* boundary, pas_segregated_page_config_variant variant)
{
    return verse_heap_segregated_page_for_page_header(verse_heap_page_header_for_boundary(boundary, variant));
}

static PAS_ALWAYS_INLINE void* verse_heap_boundary_for_segregated_page(
    pas_segregated_page* page, pas_segregated_page_config_variant variant)
{
    return verse_heap_boundary_for_page_header(verse_heap_page_header_for_segregated_page(page), variant);
}

static PAS_ALWAYS_INLINE pas_page_base* verse_heap_small_segregated_page_base_for_boundary(void* boundary)
{
    return verse_heap_page_base_for_boundary(boundary, pas_small_segregated_page_config_variant);
}

static PAS_ALWAYS_INLINE pas_page_base* verse_heap_medium_segregated_page_base_for_boundary(void* boundary)
{
    return verse_heap_page_base_for_boundary(boundary, pas_medium_segregated_page_config_variant);
}

static PAS_ALWAYS_INLINE void* verse_heap_small_segregated_boundary_for_page_base(pas_page_base* page)
{
    return verse_heap_boundary_for_page_base(page, pas_small_segregated_page_config_variant);
}

static PAS_ALWAYS_INLINE void* verse_heap_medium_segregated_boundary_for_page_base(pas_page_base* page)
{
    return verse_heap_boundary_for_page_base(page, pas_medium_segregated_page_config_variant);
}

PAS_API pas_page_base* verse_heap_small_segregated_page_base_for_boundary_remote(
    pas_enumerator* enumerator, void* boundary);
PAS_API pas_page_base* verse_heap_medium_segregated_page_base_for_boundary_remote(
    pas_enumerator* enumerator, void* boundary);
PAS_API pas_page_base* verse_heap_create_page_base(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void verse_heap_destroy_page_base(pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void* verse_heap_allocate_small_segregated_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
PAS_API void* verse_heap_allocate_medium_segregated_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
PAS_API pas_segregated_shared_page_directory* verse_heap_segregated_shared_page_directory_selector(
    pas_segregated_heap* heap, pas_segregated_size_directory* directory);

PAS_API void verse_heap_config_activate(void);
PAS_API pas_fast_megapage_kind verse_heap_config_fast_megapage_kind(uintptr_t begin);
PAS_API pas_page_base* verse_heap_config_page_base(uintptr_t begin);
PAS_API pas_aligned_allocation_result verse_heap_config_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config);
PAS_API bool verse_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg),
    void* arg);
PAS_API bool verse_heap_config_for_each_shared_page_directory_remote(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);
PAS_API void verse_heap_config_dump_shared_page_directory_arg(
    pas_stream* stream, pas_segregated_shared_page_directory* directory);

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(verse_small_segregated_page_config);
PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(verse_medium_segregated_page_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(verse_heap_config);

PAS_API extern const unsigned verse_heap_config_medium_segregated_non_committable_granule_bitvector[];

#define VERSE_HEAP_SEGREGATED_CONFIG(variant_lowercase, variant_uppercase, passed_non_committable_granule_bitvector) { \
        .base = { \
            .is_enabled = true, \
            .heap_config_ptr = &verse_heap_config, \
            .page_config_ptr = &verse_heap_config.variant_lowercase ## _segregated_config.base, \
            .page_config_kind = pas_page_config_kind_segregated, \
            .min_align_shift = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_MIN_ALIGN_SHIFT, \
            .page_size = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_PAGE_SIZE, \
            .granule_size = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_GRANULE_SIZE, \
			.non_committable_granule_bitvector = (passed_non_committable_granule_bitvector), \
            .max_object_size = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_MAX_OBJECT_SIZE, \
            .page_header_for_boundary = \
                verse_heap_ ## variant_lowercase ## _segregated_page_base_for_boundary, \
            .boundary_for_page_header = \
                verse_heap_ ## variant_lowercase ## _segregated_boundary_for_page_base, \
            .page_header_for_boundary_remote = \
                verse_heap_ ## variant_lowercase ## _segregated_page_base_for_boundary_remote, \
            .create_page_header = verse_heap_create_page_base, \
            .destroy_page_header = verse_heap_destroy_page_base, \
        }, \
        .variant = pas_ ## variant_lowercase ## _segregated_page_config_variant, \
        .kind = pas_segregated_page_config_kind_verse_ ## variant_lowercase ## _segregated, \
        .wasteage_handicap = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_WASTEAGE_HANDICAP, \
        .sharing_shift = 0, \
        .num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS( \
            VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_MIN_ALIGN_SHIFT, \
            VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_PAGE_SIZE), \
        .shared_payload_offset = 0, \
        .exclusive_payload_offset = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_HEADER_SIZE, \
        .shared_payload_size = 0, \
        .exclusive_payload_size = VERSE_HEAP_ ## variant_uppercase ## _SEGREGATED_PAYLOAD_SIZE, \
        .shared_logging_mode = pas_segregated_deallocation_no_logging_mode, \
        .exclusive_logging_mode = pas_segregated_deallocation_no_logging_mode, \
        .use_reversed_current_word = PAS_ARM64, \
        .check_deallocation = false, \
        .enable_empty_word_eligibility_optimization_for_shared = false, \
        .enable_empty_word_eligibility_optimization_for_exclusive = false, \
        .enable_view_cache = false, \
        .page_allocator = verse_heap_allocate_ ## variant_lowercase ## _segregated_page, \
        .shared_page_directory_selector = verse_heap_segregated_shared_page_directory_selector, \
        PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(verse_ ## variant_lowercase ## _segregated_page_config) \
    }

#define VERSE_HEAP_CONFIG ((pas_heap_config){ \
        .config_ptr = &verse_heap_config, \
        .kind = pas_heap_config_kind_verse, \
        .activate_callback = verse_heap_config_activate, \
        .get_type_size = verse_heap_type_get_size, \
        .get_type_alignment = verse_heap_type_get_alignment, \
        .dump_type = verse_heap_type_dump, \
        .large_alignment = VERSE_HEAP_CHUNK_SIZE, \
        .small_segregated_config = VERSE_HEAP_SEGREGATED_CONFIG(small, SMALL, NULL), \
        .medium_segregated_config = VERSE_HEAP_SEGREGATED_CONFIG(medium, MEDIUM, verse_heap_config_medium_segregated_non_committable_granule_bitvector), \
        .small_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .medium_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .marge_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_lookup_size_upper_bound = PAS_INTRINSIC_SMALL_LOOKUP_SIZE_UPPER_BOUND, \
        .fast_megapage_kind_func = verse_heap_config_fast_megapage_kind, \
        .small_segregated_is_in_megapage = false, \
        .small_bitfit_is_in_megapage = false, \
        .page_header_func = verse_heap_config_page_base, \
        .aligned_allocator = verse_heap_config_aligned_allocator, \
        .aligned_allocator_talks_to_sharing_pool = true, \
        .deallocator = NULL, \
        .mmap_capability = pas_may_mmap, \
        .root_data = NULL, \
        .prepare_to_enumerate = NULL, \
        .for_each_shared_page_directory = verse_heap_config_for_each_shared_page_directory, \
        .for_each_shared_page_directory_remote = verse_heap_config_for_each_shared_page_directory_remote, \
        .dump_shared_page_directory_arg = verse_heap_config_dump_shared_page_directory_arg, \
        PAS_HEAP_CONFIG_SPECIALIZATIONS(verse_heap_config) \
    })

PAS_API extern const pas_heap_config verse_heap_config;

static PAS_ALWAYS_INLINE bool pas_heap_config_kind_is_verse(pas_heap_config_kind kind)
{
    return kind == pas_heap_config_kind_verse;
}

static PAS_ALWAYS_INLINE bool pas_heap_config_is_verse(pas_heap_config config)
{
    return pas_heap_config_kind_is_verse(config.kind);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_CONFIG_H */

