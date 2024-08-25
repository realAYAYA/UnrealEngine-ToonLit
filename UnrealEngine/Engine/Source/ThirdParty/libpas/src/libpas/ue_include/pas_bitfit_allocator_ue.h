/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_BITFIT_ALLOCATOR_UE_H
#define PAS_BITFIT_ALLOCATOR_UE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pas_bitfit_allocator;
struct pas_bitfit_size_class;
struct pas_bitfit_view;
typedef struct pas_bitfit_allocator pas_bitfit_allocator;
typedef struct pas_bitfit_size_class pas_bitfit_size_class;
typedef struct pas_bitfit_view pas_bitfit_view;

struct pas_bitfit_allocator {
    pas_bitfit_size_class* size_class;
    pas_bitfit_view* view;
};

#ifdef __cplusplus
}
#endif

#endif /* PAS_BITFIT_ALLOCATOR_UE_H */

