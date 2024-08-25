/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_LOCAL_ALLOCATOR_UE_H
#define PAS_LOCAL_ALLOCATOR_UE_H

#include "pas_bitfit_allocator_ue.h"
#include "pas_bitvector_ue.h"
#include "pas_local_allocator_scavenger_data_ue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pas_fake_local_allocator;
struct pas_local_allocator;
typedef struct pas_fake_local_allocator pas_fake_local_allocator;
typedef struct pas_local_allocator pas_local_allocator;

/* This is only defined so that we can do offsetof on it. The fields must be matched exactly to pas_local_allocator, otherwise a bunch of runtime asserts will fail. */
struct pas_fake_local_allocator {
    pas_local_allocator_scavenger_data scavenger_data;
    
    uint8_t alignment_shift;
    enum {
        pas_fake_local_allocator_fake,
        pas_fake_local_allocator_stuff
    } config_kind : 8;
    bool current_word_is_valid;

    uintptr_t payload_end;
    unsigned remaining;
    unsigned object_size;
    uintptr_t page_ish;
    unsigned current_offset;
    unsigned end_offset;
    uint64_t current_word;
    void* view;
    uint64_t bits[1];
};

#define PAS_LOCAL_ALLOCATOR_ALIGNMENT 8

#define PAS_FAKE_LOCAL_ALLOCATOR_SIZE(num_alloc_bits) \
    (((uintptr_t)&((pas_fake_local_allocator*)0x1000)->bits - 0x1000) + \
     ((sizeof(uint64_t) * PAS_BITVECTOR_NUM_WORDS64(num_alloc_bits) > sizeof(pas_bitfit_allocator)) \
      ? sizeof(uint64_t) * PAS_BITVECTOR_NUM_WORDS64(num_alloc_bits) : sizeof(pas_bitfit_allocator)))

#ifdef __cplusplus
}
#endif

#endif /* PAS_LOCAL_ALLOCATOR_H */

