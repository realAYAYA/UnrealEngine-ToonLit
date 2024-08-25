// Copyright 2019 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_VM_BYTECODE_MODULE_IMPL_H_
#define IREE_VM_BYTECODE_MODULE_IMPL_H_

#include <stdint.h>
#include <string.h>

#include "iree/base/api.h"
#include "iree/vm/api.h"
#include "iree/vm/bytecode/utils/isa.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// A loaded bytecode module.
typedef struct iree_vm_bytecode_module_t {
  // Interface routing to the bytecode module functions.
  // Must be first in the struct as we dereference the interface to find our
  // members below.
  iree_vm_module_t interface;

  // Table of internal function bytecode descriptors.
  // Mapped 1:1 with internal functions. Each defined bytecode span represents a
  // range of bytes in |bytecode_data|.
  iree_host_size_t function_descriptor_count;
  const iree_vm_FunctionDescriptor_t* function_descriptor_table;

  // A pointer to the bytecode data embedded within the module.
  iree_const_byte_span_t bytecode_data;

  // Allocator this module was allocated with and must be freed with.
  iree_allocator_t allocator;

  // Underlying archive data and allocator (which may be null).
  iree_const_byte_span_t archive_contents;
  iree_allocator_t archive_allocator;

  // Loaded FlatBuffer module pointing into the archive contents.
  iree_vm_BytecodeModuleDef_table_t def;

  // Initialized references to rodata segments.
  iree_host_size_t rodata_ref_count;
  iree_vm_buffer_t* rodata_ref_table;

  // Type table mapping module type IDs to registered VM types.
  iree_host_size_t type_count;
  iree_vm_type_def_t type_table[];
} iree_vm_bytecode_module_t;

// A resolved and split import in the module state table.
//
// NOTE: a table of these are stored per module per context so ideally we'd
// only store the absolute minimum information to reduce our fixed overhead.
// There's a big tradeoff though as a few extra bytes here can avoid non-trivial
// work per import function invocation.
typedef struct iree_vm_bytecode_import_t {
  // Import function in the source module.
  iree_vm_function_t function;

  // Pre-parsed argument/result calling convention string fragments.
  // For example, 0ii.r will be split to arguments=ii and results=r.
  iree_string_view_t arguments;
  iree_string_view_t results;

  // Precomputed argument/result size requirements for marshaling values.
  // Only usable for non-variadic signatures. Results are always usable as they
  // don't support variadic values (yet).
  uint16_t argument_buffer_size;
  uint16_t result_buffer_size;
} iree_vm_bytecode_import_t;

// Per-instance module state.
// This is allocated with a provided allocator as a single flat allocation.
// This struct is a prefix to the allocation pointing into the dynamic offsets
// of the allocation storage.
typedef struct iree_vm_bytecode_module_state_t {
  // Combined rwdata storage for the entire module, including globals.
  // Aligned to 16 bytes (128-bits) for SIMD usage.
  iree_byte_span_t rwdata_storage;

  // Global ref values, indexed by global ordinal.
  iree_host_size_t global_ref_count;
  iree_vm_ref_t* global_ref_table;

  // Resolved function imports.
  iree_host_size_t import_count;
  iree_vm_bytecode_import_t* import_table;

  // Allocator used for the state itself and any runtime allocations needed.
  iree_allocator_t allocator;
} iree_vm_bytecode_module_state_t;

// Begins execution of the current frame and continues until either a yield or
// return.
iree_status_t iree_vm_bytecode_dispatch_begin(
    iree_vm_stack_t* stack, iree_vm_bytecode_module_t* module,
    const iree_vm_function_call_t call, iree_string_view_t cconv_arguments,
    iree_string_view_t cconv_results);

// Resumes execution of an in-progress frame and continues until either a yield
// or return.
iree_status_t iree_vm_bytecode_dispatch_resume(
    iree_vm_stack_t* stack, iree_vm_bytecode_module_t* module,
    iree_byte_span_t call_results);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_VM_BYTECODE_MODULE_IMPL_H_
