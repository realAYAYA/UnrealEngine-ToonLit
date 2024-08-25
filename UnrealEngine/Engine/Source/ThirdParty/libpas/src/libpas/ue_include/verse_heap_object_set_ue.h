/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_OBJECT_SET_UE_H
#define VERSE_HEAP_OBJECT_SET_UE_H

#include <stddef.h>
#include "verse_heap_iterate_filter_ue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct verse_heap_object_set;
typedef struct verse_heap_object_set verse_heap_object_set;

PAS_API verse_heap_object_set* verse_heap_object_set_create(void);

PAS_API void verse_heap_object_set_start_iterate_before_handshake(verse_heap_object_set* set);
PAS_API size_t verse_heap_object_set_start_iterate_after_handshake(verse_heap_object_set* set);
PAS_API void verse_heap_object_set_iterate_range(
    verse_heap_object_set* set,
    size_t begin,
    size_t end,
    verse_heap_iterate_filter filter,
    void (*callback)(void* object, void* arg),
    void* arg);
PAS_API void verse_heap_object_set_end_iterate(verse_heap_object_set* set);

#ifdef __cplusplus
}
#endif

#endif /* VERSE_HEAP_OBJECT_SET_UE_H */

