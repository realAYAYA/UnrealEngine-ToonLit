/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestHarness.h"
#include "bmalloc_heap.h"
#include "pas_scavenger.h"

#include <cstdlib>

using namespace std;

namespace {

void testBmallocAllocate()
{
    void* mem = bmalloc_try_allocate(100);
    CHECK(mem);
}

void testBmallocDeallocate()
{
    void* mem = bmalloc_try_allocate(100);
    CHECK(mem);
    bmalloc_deallocate(mem);
}

void testReallocateWithAlignment(size_t initialSize, size_t initialAlignment, size_t newSize, size_t newAlignment, size_t numRepeats)
{
    PAS_ASSERT(pas_is_power_of_2(initialAlignment));
    PAS_ASSERT(pas_is_power_of_2(newAlignment));
    for (size_t index = 0; index < numRepeats; ++index) {
        void* ptr = bmalloc_allocate_with_alignment(initialSize, initialAlignment);
        CHECK(ptr);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), initialAlignment));
        ptr = bmalloc_reallocate_with_alignment(ptr, newSize, newAlignment, pas_reallocate_free_if_successful);
        CHECK(ptr);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), newAlignment));
    }
}

void testReallocateMallocs(size_t size)
{
	void* ptr = bmalloc_reallocate(nullptr, size, pas_reallocate_free_if_successful);
	CHECK(ptr);
	bmalloc_deallocate(ptr);
}

void testReallocateFrees(size_t size)
{
	void* ptr = bmalloc_allocate(size);
	void* ptr2 = bmalloc_reallocate(ptr, 0, pas_reallocate_free_if_successful);
	CHECK(!ptr2);
	pas_scavenger_clear_local_tlcs();
	void* ptr3 = bmalloc_allocate(size);
	CHECK_EQUAL(ptr3, ptr);
}

} // anonymous namespace

void addBmallocTests()
{
    ADD_TEST(testBmallocAllocate());
    ADD_TEST(testBmallocDeallocate());
    ADD_TEST(testReallocateWithAlignment(16, 16, 32, 32, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 64, 64, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 128, 128, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 256, 256, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 512, 512, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 1024, 1024, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 2048, 2048, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 4096, 4096, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 8192, 8192, 1000));
    ADD_TEST(testReallocateWithAlignment(16, 16, 16384, 16384, 100));
    ADD_TEST(testReallocateWithAlignment(16, 16, 32768, 32768, 100));
    ADD_TEST(testReallocateWithAlignment(16, 16, 65536, 65536, 100));
    ADD_TEST(testReallocateWithAlignment(16, 16, 131072, 131072, 100));
    ADD_TEST(testReallocateWithAlignment(512, 256, 1024, 1024, 1000));
    ADD_TEST(testReallocateWithAlignment(512, 256, 2048, 2048, 1000));
    ADD_TEST(testReallocateWithAlignment(512, 256, 4096, 4096, 1000));
    ADD_TEST(testReallocateWithAlignment(512, 256, 8192, 8192, 1000));
    ADD_TEST(testReallocateWithAlignment(512, 256, 16384, 16384, 100));
    ADD_TEST(testReallocateWithAlignment(512, 256, 32768, 32768, 100));
    ADD_TEST(testReallocateWithAlignment(512, 256, 65536, 65536, 100));
    ADD_TEST(testReallocateWithAlignment(512, 256, 131072, 131072, 100));
	ADD_TEST(testReallocateMallocs(0));
	ADD_TEST(testReallocateMallocs(16));
	ADD_TEST(testReallocateMallocs(32));
	ADD_TEST(testReallocateMallocs(1000));
	ADD_TEST(testReallocateMallocs(10000));
	ADD_TEST(testReallocateMallocs(10000000));
	ADD_TEST(testReallocateFrees(0));
	ADD_TEST(testReallocateFrees(16));
	ADD_TEST(testReallocateFrees(32));
	ADD_TEST(testReallocateFrees(1000));
	ADD_TEST(testReallocateFrees(10000));
	ADD_TEST(testReallocateFrees(10000000));
}
