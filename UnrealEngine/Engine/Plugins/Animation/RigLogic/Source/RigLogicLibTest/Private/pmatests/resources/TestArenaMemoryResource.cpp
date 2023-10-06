// Copyright Epic Games, Inc. All Rights Reserved.

#include "pmatests/Defs.h"

#include <pma/resources/ArenaMemoryResource.h>
#include <pma/resources/DefaultMemoryResource.h>

#include <cstddef>
#include <cstdint>

namespace {

struct TransparentMemoryResource : public pma::DefaultMemoryResource {

    struct Allocation {
        std::size_t size;
        std::size_t alignment;
    };

    struct Deallocation {
        void* ptr;
        std::size_t size;
        std::size_t alignment;
    };

    struct Expected {
        std::size_t size;
        std::size_t count;
    };

    TransparentMemoryResource() :
        allocationCount{},
        deallocationCount{},
        lastAllocation{},
        lastDeallocation{},
        expectedAllocation{},
        expectedDeallocation{} {
    }

    void* allocate(std::size_t size, std::size_t alignment) override {
        lastAllocation = Allocation{size, alignment};
        ++allocationCount;
        if (size == expectedAllocation.size) {
            ++expectedAllocation.count;
        }
        return DefaultMemoryResource::allocate(size, alignment);
    }

    void deallocate(void* ptr, std::size_t size, std::size_t alignment) override {
        lastDeallocation = Deallocation{ptr, size, alignment};
        ++deallocationCount;
        if (size == expectedDeallocation.size) {
            ++expectedDeallocation.count;
        }
        DefaultMemoryResource::deallocate(ptr, size, alignment);
    }

    void expectAllocation(std::size_t size) {
        expectedAllocation.size = size;
        expectedAllocation.count = 0ul;
    }

    void expectDeallocation(std::size_t size) {
        expectedDeallocation.size = size;
        expectedDeallocation.count = 0ul;
    }

    std::size_t allocationCount;
    std::size_t deallocationCount;
    Allocation lastAllocation;
    Deallocation lastDeallocation;
    Expected expectedAllocation;
    Expected expectedDeallocation;

};

}  // namespace

TEST(ArenaMemoryResourceTest, EnsureArenaServesAllocations) {
    TransparentMemoryResource upstream;
    ASSERT_EQ(upstream.allocationCount, 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);

    upstream.expectAllocation(256ul);

    pma::ArenaMemoryResource amr{256ul, &upstream};
    // The inner structure of the arena memory resource are allocated through upstream
    // as well, thus there are more than 1 allocation at this point
    ASSERT_TRUE(upstream.allocationCount > 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);
    ASSERT_EQ(upstream.expectedAllocation.count, 1ul);

    // Make sure no further allocations happen from this point
    upstream.allocationCount = 0ul;

    amr.allocate(128ul, 4ul);
    ASSERT_EQ(upstream.allocationCount, 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);

    amr.allocate(128ul, 4ul);
    ASSERT_EQ(upstream.allocationCount, 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);

    // This allocation should trigger a new region allocation since the existing one is exhausted
    amr.allocate(1ul, 1ul);
    ASSERT_TRUE(upstream.allocationCount > 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);
    ASSERT_EQ(upstream.expectedAllocation.count, 2ul);
}

TEST(ArenaMemoryResourceTest, EnsureDeallocationAreNoop) {
    TransparentMemoryResource upstream;
    ASSERT_EQ(upstream.deallocationCount, 0ul);

    pma::ArenaMemoryResource amr{256ul, &upstream};

    auto a1 = amr.allocate(128ul, 4ul);
    auto a2 = amr.allocate(64ul, 4ul);
    auto a3 = amr.allocate(32ul, 4ul);

    amr.deallocate(a3, 32ul, 4ul);
    amr.deallocate(a2, 64ul, 4ul);
    amr.deallocate(a1, 128ul, 4ul);

    ASSERT_EQ(upstream.deallocationCount, 0ul);
}

TEST(ArenaMemoryResourceTest, EnsureGrowthFactorIsHonored) {
    TransparentMemoryResource upstream;
    ASSERT_EQ(upstream.allocationCount, 0ul);
    ASSERT_EQ(upstream.deallocationCount, 0ul);

    // Allocation the first region
    upstream.expectAllocation(256ul);
    pma::ArenaMemoryResource amr{256ul, 128ul, 3.0f, &upstream};
    ASSERT_EQ(upstream.expectedAllocation.count, 1ul);

    // No new allocations should happen
    auto prevAllocationCount = upstream.allocationCount;
    amr.allocate(256ul, 4ul);
    ASSERT_EQ(prevAllocationCount, upstream.allocationCount);

    // Allocate first additional region (==regionSize)
    upstream.expectAllocation(128ul);
    amr.allocate(128ul, 4ul);
    ASSERT_EQ(upstream.expectedAllocation.count, 1ul);

    // Allocate new 3x larger region
    upstream.expectAllocation(384ul);
    amr.allocate(256ul, 4ul);
    ASSERT_EQ(upstream.expectedAllocation.count, 1ul);

    // No new allocations should happen
    prevAllocationCount = upstream.allocationCount;
    amr.allocate(64ul, 4ul);
    amr.allocate(64ul, 4ul);
    ASSERT_EQ(prevAllocationCount, upstream.allocationCount);

    // Allocate new 3x larger region
    upstream.expectAllocation(1152ul);
    amr.allocate(256ul, 4ul);
    ASSERT_EQ(upstream.expectedAllocation.count, 1ul);
}

TEST(ArenaMemoryResourceTest, EnsureAlignmentRequirementsAreHonored) {
    TransparentMemoryResource upstream;
    pma::ArenaMemoryResource amr{1024ul, &upstream};

    auto p1 = reinterpret_cast<std::uintptr_t>(amr.allocate(7ul, 4ul));
    ASSERT_EQ(p1 % 4ul, 0ul);

    auto p2 = reinterpret_cast<std::uintptr_t>(amr.allocate(13ul, 4ul));
    ASSERT_EQ(p2 % 4ul, 0ul);

    auto p3 = reinterpret_cast<std::uintptr_t>(amr.allocate(27ul, 16ul));
    ASSERT_EQ(p3 % 16ul, 0ul);

    auto p4 = reinterpret_cast<std::uintptr_t>(amr.allocate(41ul, 16ul));
    ASSERT_EQ(p4 % 16ul, 0ul);
}
