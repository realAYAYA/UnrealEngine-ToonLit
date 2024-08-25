/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_LOCAL_ALLOCATOR_LOCATION_H
#define PAS_LOCAL_ALLOCATOR_LOCATION_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_local_allocator_location {
    pas_local_allocator_in_thread_local_cache, /* This has to be zero. */
    pas_local_allocator_not_in_thread_local_cache
};

typedef enum pas_local_allocator_location pas_local_allocator_location;

static inline const char* pas_local_allocator_location_get_string(pas_local_allocator_location location)
{
    switch (location) {
    case pas_local_allocator_in_thread_local_cache:
        return "in_thread_local_cache";
    case pas_local_allocator_not_in_thread_local_cache:
        return "not_in_thread_local_cache";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_LOCATION_H */

