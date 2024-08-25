// Copyright 2021 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_HAL_DRIVERS_CUDA_GRAPH_COMMAND_BUFFER_H_
#define IREE_HAL_DRIVERS_CUDA_GRAPH_COMMAND_BUFFER_H_

#include "iree/base/api.h"
#include "iree/hal/api.h"
#include "iree/hal/drivers/cuda/context_wrapper.h"
#include "iree/hal/drivers/cuda/cuda_headers.h"
#include "iree/hal/drivers/cuda/dynamic_symbols.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct iree_arena_block_pool_t iree_arena_block_pool_t;

// Creates a command buffer that records into a CUDA graph.
//
// NOTE: the |block_pool| must remain live for the lifetime of the command
// buffers that use it.
iree_status_t iree_hal_cuda_graph_command_buffer_create(
    iree_hal_device_t* device, iree_hal_cuda_context_wrapper_t* context,
    iree_hal_command_buffer_mode_t mode,
    iree_hal_command_category_t command_categories,
    iree_hal_queue_affinity_t queue_affinity, iree_host_size_t binding_capacity,
    iree_arena_block_pool_t* block_pool,
    iree_hal_command_buffer_t** out_command_buffer);

// Returns true if |command_buffer| is a CUDA graph-based command buffer.
bool iree_hal_cuda_graph_command_buffer_isa(
    iree_hal_command_buffer_t* command_buffer);

// Returns the native cuda graph associated to the command buffer.
CUgraphExec iree_hal_cuda_graph_command_buffer_handle(
    iree_hal_command_buffer_t* command_buffer);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_HAL_DRIVERS_CUDA_GRAPH_COMMAND_BUFFER_H_
