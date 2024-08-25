/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_TYPE_H
#define VERSE_HEAP_TYPE_H

#include "pas_heap_ref.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct pas_stream;
typedef struct pas_stream pas_stream;

static inline pas_heap_type* verse_heap_type_create(size_t alignment)
{
    return (pas_heap_type*)alignment;
}

static inline size_t verse_heap_type_get_size(const pas_heap_type* type)
{
    return (size_t)type;
}

static inline size_t verse_heap_type_get_alignment(const pas_heap_type* type)
{
    return (size_t)type;
}

PAS_API void verse_heap_type_dump(const pas_heap_type* type, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_TYPE_H */

