/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_UE_H
#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_UE_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pas_local_allocator_scavenger_data;
typedef struct pas_local_allocator_scavenger_data pas_local_allocator_scavenger_data;

/* This should just be 32-bit. */
struct pas_local_allocator_scavenger_data {
    bool is_in_use;
    uint8_t should_stop_count;
    bool dirty;
    uint8_t encoded_kind_and_location;
};

#ifdef __cplusplus
}
#endif

#endif /* PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_UE_H */

