// Copyright 2020 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// NOTE: must be first to ensure that we can define settings for all includes.
#include "iree/base/internal/wait_handle_impl.h"

#ifndef IREE_BASE_INTERNAL_WAIT_HANDLE_POSIX_H_
#define IREE_BASE_INTERNAL_WAIT_HANDLE_POSIX_H_

#if defined(IREE_WAIT_API_POSIX_LIKE)

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Perform a syscall with a retry on EINTR (spurious wake/signal/etc).
//
// Usage:
//  int rv;
//  IREE_SYSCALL(rv, fcntl(...));
//  if (rv < 0) { /* failure */ }
#define IREE_SYSCALL(result_value, expr) \
  do {                                   \
    result_value = expr;                 \
  } while (result_value < 0 && errno == EINTR);

// NOTE: these are intended for low-level signaling and may expose various
// platform quirks to the caller. Always prefer using a higher level type such
// as iree_event_t when possible.

// Creates a wait primitive of the type native to the current platform.
// May fail if resources are exhausted or wait handles are not supported.
// The handle must be closed with iree_wait_handle_close to release its
// resources.
iree_status_t iree_wait_primitive_create_native(bool initial_state,
                                                iree_wait_handle_t* out_handle);

// Closes an existing handle from iree_wait_primitive_create_native or
// iree_wait_primitive_clone. Must not be called while there are any waiters on
// the handle.
void iree_wait_handle_close(iree_wait_handle_t* handle);

// Returns true if the two handles are identical in representation.
// Note that two unique handles may point to the same underlying primitive
// object (such as when they have been cloned).
bool iree_wait_primitive_compare_identical(const iree_wait_handle_t* lhs,
                                           const iree_wait_handle_t* rhs);

// Returns an fd that can be used to read/wait on the handle.
// Returns -1 if the handle is invalid.
int iree_wait_primitive_get_read_fd(const iree_wait_handle_t* handle);

// Reads a nonce from the given handle and blocks the caller if none are
// available. IREE_TIME_INFINITE_PAST can be used to poll (the call will never
// block) and IREE_TIME_INFINITE_FUTURE can be used to block until the primitive
// is written.
iree_status_t iree_wait_primitive_read(iree_wait_handle_t* handle,
                                       iree_time_t deadline_ns);

// Writes a nonce to the given handle causing it to signal any waiters.
// The exact value written is platform/primitive specific.
iree_status_t iree_wait_primitive_write(iree_wait_handle_t* handle);

// Clears the wait primitive by repeatedly reading values until no more remain.
// Never blocks the caller.
iree_status_t iree_wait_primitive_clear(iree_wait_handle_t* handle);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_WAIT_API_POSIX_LIKE

#endif  // IREE_BASE_INTERNAL_WAIT_HANDLE_POSIX_H_
