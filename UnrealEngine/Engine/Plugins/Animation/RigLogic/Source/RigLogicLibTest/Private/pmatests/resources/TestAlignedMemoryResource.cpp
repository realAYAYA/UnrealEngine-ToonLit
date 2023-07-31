// Copyright Epic Games, Inc. All Rights Reserved.

#include "pmatests/Defs.h"

#include <pma/resources/AlignedMemoryResource.h>

#include <cstdint>

namespace {

class AlignedMemoryResourceTest : public ::testing::TestWithParam<std::size_t> {
};

}  // namespace

TEST_P(AlignedMemoryResourceTest, AlignmentRequirementHonored) {
    std::size_t allocSize = 1024u;
    std::size_t alignment = GetParam();
    pma::AlignedMemoryResource amr;
    void* ptr = amr.allocate(allocSize, alignment);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1), static_cast<std::uintptr_t>(0));
    amr.deallocate(ptr, allocSize, alignment);
}

INSTANTIATE_TEST_SUITE_P(AlignedMemoryResourceTest, AlignedMemoryResourceTest, ::testing::Values(8, 64, 1024, 4096));
