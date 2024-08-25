// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_BUILTINS_UKERNEL_TOOLS_TEST_H_
#define IREE_BUILTINS_UKERNEL_TOOLS_TEST_H_

#include "iree/builtins/ukernel/tools/util.h"

// Opaque struct holding some test case information and some resources such as
// a pseudorandom engine and CPU data.
typedef struct iree_uk_test_t iree_uk_test_t;

// Synchronously run a test --- no registration.
//
// `params` is the "user data" parameters that will be passed as the second
// argument to `test_func`.
//
// If `cpu_features` is non-NULL then `test_func` will be called a second time
// with the corresponding CPU features enabled in the iree_uk_test_t* passed as
// first argument to `test_func`. This is done as a second separate call to
// `test_func` so that we maintain test coverage for the fallback logic and
// baseline kernels used when CPU features are unavailable.
void iree_uk_test(const char* name,
                  void (*test_func)(iree_uk_test_t*, const void*),
                  const void* params, const char* cpu_features);

// Fail the current test.
#define IREE_UK_TEST_FAIL(test) iree_uk_test_fail(test, __FILE__, __LINE__)

// Implementation of IREE_UK_TEST_FAIL.
void iree_uk_test_fail(iree_uk_test_t* test, const char* file, int line);

// Used by test functions to get a random engine.
iree_uk_random_engine_t* iree_uk_test_random_engine(const iree_uk_test_t* test);

// Used by test functions to get CPU data.
const iree_uk_uint64_t* iree_uk_test_cpu_data(const iree_uk_test_t* test);

// Must be called by the test `main` function to provide its return value.
int iree_uk_test_exit_status(void);

#endif  // IREE_BUILTINS_UKERNEL_TOOLS_TEST_H_
