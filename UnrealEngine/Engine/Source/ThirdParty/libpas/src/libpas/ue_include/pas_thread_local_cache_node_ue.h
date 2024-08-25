/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_THREAD_LOCAL_CACHE_NODE_UE_H
#define PAS_THREAD_LOCAL_CACHE_NODE_UE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pas_thread_local_cache_node;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;

PAS_API uint64_t pas_thread_local_cache_node_version(pas_thread_local_cache_node* node);

#ifdef __cplusplus
}
#endif

#endif /* PAS_THREAD_LOCAL_CACHE_NODE_UE_H */

